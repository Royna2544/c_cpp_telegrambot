#include <absl/log/log.h>
#include <fmt/format.h>

#include <algorithm>
#include <api/components/ModuleExecutionContext.hpp>
#include <api/components/WorkScheduler.hpp>
#include <chrono>
#include <exception>
#include <map>
#include <stdexcept>
#include <utility>

using namespace std::chrono_literals;

class TgBotApiImpl::WorkScheduler::Lane {
   public:
    Lane(std::string name, std::size_t workers, std::size_t queuedCapacity,
         std::chrono::milliseconds defaultDeadline)
        : name_(std::move(name)),
          maximumOutstanding_(workers + queuedCapacity),
          defaultDeadline_(defaultDeadline) {
        if (workers == 0 || maximumOutstanding_ == 0) {
            throw std::invalid_argument("work lane requires capacity");
        }
        for (std::size_t i = 0; i < workers; ++i) {
            workers_.emplace_back([this] { workerLoop(); });
        }
        deadlineMonitor_ = std::thread([this] { monitorDeadlines(); });
        LOG(INFO) << fmt::format("Started work lane '{}' workers={} queued={}",
                                 name_, workers, queuedCapacity);
    }

    ~Lane() {
        {
            const std::lock_guard lock(mutex_);
            stopping_ = true;
            for (auto& [id, task] : active_)
                task->stop.request_stop();
            queue_.clear();
        }
        ready_.notify_all();
        drained_.notify_all();
        if (deadlineMonitor_.joinable())
            deadlineMonitor_.join();
        for (auto& worker : workers_) {
            if (worker.joinable())
                worker.join();
        }
    }

    struct Task {
        WorkId id{};
        std::string owner;
        // Keep the module loaded until after the callable (whose destructor may
        // live in that module) has been destroyed. Members are destroyed in
        // reverse declaration order, so the lease must precede the callable.
        std::shared_ptr<void> moduleLease;
        Work work;
        std::stop_source stop;
        std::chrono::steady_clock::time_point due;
        std::chrono::steady_clock::time_point deadline;
        std::atomic_bool deadlineReported{};
    };

    [[nodiscard]] bool submit(std::shared_ptr<Task> task) {
        std::size_t depth = 0;
        {
            const std::lock_guard lock(mutex_);
            if (stopping_ ||
                queue_.size() + active_.size() >= maximumOutstanding_) {
                LOG(WARNING) << fmt::format(
                    "Work lane '{}' rejected owner={} depth={}", name_,
                    task->owner, queue_.size() + active_.size());
                return false;
            }
            queue_.emplace(task->due, std::move(task));
            depth = queue_.size() + active_.size();
        }
        LOG(INFO) << fmt::format("Work lane '{}' queued depth={}", name_,
                                 depth);
        ready_.notify_all();
        return true;
    }

    [[nodiscard]] bool cancel(std::string_view owner, WorkId id) {
        const std::lock_guard lock(mutex_);
        for (auto iter = queue_.begin(); iter != queue_.end(); ++iter) {
            if (iter->second->id == id && iter->second->owner == owner) {
                iter->second->stop.request_stop();
                queue_.erase(iter);
                drained_.notify_all();
                LOG(INFO) << fmt::format(
                    "Cancelled queued work lane='{}' owner={} id={}", name_,
                    owner, id);
                return true;
            }
        }
        const auto active = active_.find(id);
        if (active != active_.end() && active->second->owner == owner) {
            active->second->stop.request_stop();
            LOG(INFO) << fmt::format(
                "Cancellation requested lane='{}' owner={} id={}", name_, owner,
                id);
            return true;
        }
        return false;
    }

    void cancelOwner(std::string_view owner) {
        const std::lock_guard lock(mutex_);
        std::erase_if(queue_, [owner](const auto& entry) {
            if (entry.second->owner != owner)
                return false;
            entry.second->stop.request_stop();
            return true;
        });
        for (auto& [id, task] : active_) {
            if (task->owner == owner)
                task->stop.request_stop();
        }
        ready_.notify_all();
        drained_.notify_all();
    }

    void drainOwner(std::string_view owner) {
        std::unique_lock lock(mutex_);
        drained_.wait(lock, [&] {
            const bool queued =
                std::ranges::any_of(queue_, [owner](const auto& entry) {
                    return entry.second->owner == owner;
                });
            const bool active =
                std::ranges::any_of(active_, [owner](const auto& entry) {
                    return entry.second->owner == owner;
                });
            return !queued && !active;
        });
    }

    [[nodiscard]] std::size_t depth() const {
        const std::lock_guard lock(mutex_);
        return queue_.size() + active_.size();
    }

    [[nodiscard]] std::chrono::milliseconds defaultDeadline() const {
        return defaultDeadline_;
    }

   private:
    void workerLoop() {
        while (true) {
            std::shared_ptr<Task> task;
            {
                std::unique_lock lock(mutex_);
                while (!stopping_) {
                    if (queue_.empty()) {
                        ready_.wait(lock);
                        continue;
                    }
                    const auto due = queue_.begin()->first;
                    if (std::chrono::steady_clock::now() < due) {
                        ready_.wait_until(lock, due);
                        continue;
                    }
                    task = std::move(queue_.begin()->second);
                    queue_.erase(queue_.begin());
                    active_.emplace(task->id, task);
                    break;
                }
                if (stopping_ && !task)
                    return;
            }

            const auto started = std::chrono::steady_clock::now();
            if (task->deadline !=
                    std::chrono::steady_clock::time_point::max() &&
                started >= task->deadline) {
                task->deadlineReported = true;
                task->stop.request_stop();
                LOG(WARNING) << fmt::format(
                    "Work expired before execution lane='{}' owner={} id={}",
                    name_, task->owner, task->id);
            }
            try {
                if (!task->stop.stop_requested()) {
                    module_execution::Scope active(task->owner);
                    task->work(task->stop.get_token());
                }
            } catch (const TgBot::TgException& error) {
                LOG(ERROR) << fmt::format(
                    "Work lane '{}' owner={} id={} Telegram exception: {}",
                    name_, task->owner, task->id, error.what());
            } catch (const std::exception& error) {
                LOG(ERROR) << fmt::format(
                    "Work lane '{}' owner={} id={} exception: {}", name_,
                    task->owner, task->id, error.what());
            } catch (...) {
                LOG(ERROR) << fmt::format(
                    "Work lane '{}' owner={} id={} unknown exception", name_,
                    task->owner, task->id);
            }
            // Destroy a module-defined closure while its execution lease is
            // still held. This is also required when unload is waiting in
            // cancelAndDrain(): erasing the active task may release the final
            // lease and permit dlclose immediately.
            task->work = {};
            const auto elapsed =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - started);
            const bool cancelled = task->stop.stop_requested();
            {
                const std::lock_guard lock(mutex_);
                active_.erase(task->id);
            }
            drained_.notify_all();
            LOG(INFO) << fmt::format(
                "Work lane '{}' owner={} id={} duration_ms={} cancelled={}",
                name_, task->owner, task->id, elapsed.count(), cancelled);
        }
    }

    void monitorDeadlines() {
        std::unique_lock lock(mutex_);
        while (!stopping_) {
            ready_.wait_for(lock, 100ms);
            if (stopping_)
                break;
            const auto now = std::chrono::steady_clock::now();
            for (auto& [id, task] : active_) {
                if (task->deadline ==
                        std::chrono::steady_clock::time_point::max() ||
                    now < task->deadline || task->deadlineReported) {
                    continue;
                }
                task->deadlineReported = true;
                task->stop.request_stop();
                LOG(WARNING) << fmt::format(
                    "Work deadline exceeded lane='{}' owner={} id={}", name_,
                    task->owner, id);
            }
        }
    }

    std::string name_;
    std::size_t maximumOutstanding_{};
    std::chrono::milliseconds defaultDeadline_{};
    mutable std::mutex mutex_;
    std::condition_variable ready_;
    std::condition_variable drained_;
    bool stopping_{};
    std::multimap<std::chrono::steady_clock::time_point, std::shared_ptr<Task>>
        queue_;
    std::unordered_map<WorkId, std::shared_ptr<Task>> active_;
    std::vector<std::thread> workers_;
    std::thread deadlineMonitor_;
};

TgBotApiImpl::WorkScheduler::WorkScheduler()
    : llm_(std::make_unique<Lane>("llm", 2, 8, 180s)),
      media_(std::make_unique<Lane>("media", 1, 2, 60s)),
      process_(std::make_unique<Lane>("process", 1, 4, 10s)),
      outbound_(std::make_unique<Lane>("outbound", 1, 64, 30s)),
      unboundedProcess_(std::make_unique<Lane>("ubash", 1, 0, 0ms)) {}

TgBotApiImpl::WorkScheduler::~WorkScheduler() = default;

TgBotApiImpl::WorkScheduler::Lane& TgBotApiImpl::WorkScheduler::lane(
    WorkClass workClass) {
    return const_cast<Lane&>(std::as_const(*this).lane(workClass));
}

const TgBotApiImpl::WorkScheduler::Lane& TgBotApiImpl::WorkScheduler::lane(
    WorkClass workClass) const {
    switch (workClass) {
        case WorkClass::Llm:
            return *llm_;
        case WorkClass::Media:
            return *media_;
        case WorkClass::Process:
            return *process_;
        case WorkClass::Outbound:
            return *outbound_;
        case WorkClass::UnboundedProcess:
            return *unboundedProcess_;
    }
    throw std::invalid_argument("unknown work class");
}

std::optional<TgBotApiImpl::WorkScheduler::WorkId>
TgBotApiImpl::WorkScheduler::submit(std::string owner, WorkClass workClass,
                                    Work work, WorkOptions options,
                                    std::shared_ptr<void> moduleLease) {
    if (owner.empty() || !work)
        return std::nullopt;
    auto& target = lane(workClass);
    const auto now = std::chrono::steady_clock::now();
    const auto deadlineDuration = options.deadline.count() > 0
                                      ? options.deadline
                                      : target.defaultDeadline();
    const auto id = nextId_.fetch_add(1, std::memory_order_relaxed);
    auto task = std::make_shared<Lane::Task>();
    task->id = id;
    task->owner = std::move(owner);
    task->work = std::move(work);
    task->due = now + std::max(options.delay, 0ms);
    task->deadline = deadlineDuration.count() > 0
                         ? task->due + deadlineDuration
                         : std::chrono::steady_clock::time_point::max();
    task->moduleLease = std::move(moduleLease);
    if (!target.submit(std::move(task)))
        return std::nullopt;
    return id;
}

bool TgBotApiImpl::WorkScheduler::cancel(std::string_view owner, WorkId id) {
    return llm_->cancel(owner, id) || media_->cancel(owner, id) ||
           process_->cancel(owner, id) || outbound_->cancel(owner, id) ||
           unboundedProcess_->cancel(owner, id);
}

void TgBotApiImpl::WorkScheduler::cancelAndDrain(std::string_view owner) {
    // Request cancellation everywhere before waiting anywhere. Otherwise an
    // uncooperative job in the first lane could leave the same module's
    // process/media work running throughout a blocked unload.
    llm_->cancelOwner(owner);
    media_->cancelOwner(owner);
    process_->cancelOwner(owner);
    outbound_->cancelOwner(owner);
    unboundedProcess_->cancelOwner(owner);

    llm_->drainOwner(owner);
    media_->drainOwner(owner);
    process_->drainOwner(owner);
    outbound_->drainOwner(owner);
    unboundedProcess_->drainOwner(owner);
}

std::size_t TgBotApiImpl::WorkScheduler::depth(WorkClass workClass) const {
    return lane(workClass).depth();
}
