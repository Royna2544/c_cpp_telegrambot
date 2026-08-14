#include <api/TgBotApiImpl.hpp>
#include <api/components/Async.hpp>
#include <stdexcept>
#include <string_view>

namespace {
thread_local TgBotApiImpl::Async* currentExecutor = nullptr;
thread_local const std::string* currentTaskOwner = nullptr;
}  // namespace

bool TgBotApiImpl::Async::emplaceTask(std::string command,
                                      std::function<void()> task) {
    return emplaceTaskIf(std::move(command), std::move(task), {}) ==
           EnqueueResult::Accepted;
}

TgBotApiImpl::Async::EnqueueResult TgBotApiImpl::Async::emplaceTaskIf(
    std::string command, std::function<void()> task,
    const std::function<bool()>& admission) {
    std::unique_lock<std::mutex> lock(mutex);
    if (stopWorker || blockedOwners.contains(command) ||
        tasks.size() >= maxQueueSize) {
        return EnqueueResult::QueueFullOrStopping;
    }
    // Capacity is known to be available. Commit quota (or another admission
    // reservation) immediately before adding the task, under the same queue
    // lock, so failed validation/full queues never consume a reservation.
    if (admission && !admission()) {
        return EnqueueResult::Rejected;
    }
    tasks.emplace(std::move(command), std::move(task));
    lock.unlock();
    condVariable.notify_one();
    return EnqueueResult::Accepted;
}

TgBotApiImpl::Async::Async(std::string name, const int count,
                           const std::size_t maxQueueSize)
    : maxQueueSize(maxQueueSize), _name(std::move(name)) {
    if (count <= 0 || maxQueueSize == 0) {
        throw std::invalid_argument(
            "Async requires at least one worker and one queue slot");
    }
    DLOG(INFO) << fmt::format("Starting AsyncThreads '{}', count: {}", _name,
                              count);
    for (int i = 0; i < count; ++i) {
        threads.emplace_back([this]() { threadFunction(); });
    }
}

TgBotApiImpl::Async::~Async() {
    DLOG(INFO) << fmt::format("Stopping AsyncThreads '{}'", _name);
    stopWorker = true;
    condVariable.notify_all();
    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    threads.clear();
}

void TgBotApiImpl::Async::cancel(const std::string_view owner) {
    if (owner.empty()) {
        return;
    }

    std::queue<Task> retained;
    std::vector<Task> retired;
    {
        const std::lock_guard lock(mutex);
        ++blockedOwners[std::string(owner)];
        retired.reserve(tasks.size());
        while (!tasks.empty()) {
            auto task = std::move(tasks.front());
            tasks.pop();
            if (task.first == owner) {
                retired.emplace_back(std::move(task));
            } else {
                retained.emplace(std::move(task));
            }
        }
        tasks.swap(retained);
    }

    // A queued closure can own a shared module execution lease and its
    // std::function manager can be tied to that module. Destroy it outside the
    // executor mutex, but synchronously before the module loader may dlclose.
    retired.clear();
    drainedVariable.notify_all();
}

void TgBotApiImpl::Async::drain(const std::string_view owner) {
    if (owner.empty()) {
        return;
    }

    const std::size_t currentAllowance = currentExecutor == this &&
                                                 currentTaskOwner != nullptr &&
                                                 *currentTaskOwner == owner
                                             ? 1U
                                             : 0U;

    std::unique_lock lock(mutex);
    drainedVariable.wait(lock, [&] {
        const auto active = activeTasks.find(std::string(owner));
        return active == activeTasks.end() ||
               active->second <= currentAllowance;
    });
    const auto blocked = blockedOwners.find(std::string(owner));
    if (blocked != blockedOwners.end()) {
        if (blocked->second <= 1) {
            blockedOwners.erase(blocked);
        } else {
            --blocked->second;
        }
    }
}

void TgBotApiImpl::Async::cancelAndDrain(const std::string_view owner) {
    cancel(owner);
    drain(owner);
}

void TgBotApiImpl::Async::threadFunction() {
    while (true) {
        std::unique_lock<std::mutex> lock(mutex);
        condVariable.wait(lock,
                          [this] { return !tasks.empty() || stopWorker; });
        if (tasks.empty()) {
            if (stopWorker) {
                return;
            }
            continue;
        }

        auto front = std::move(tasks.front());
        tasks.pop();
        ++activeTasks[front.first];
        lock.unlock();

        auto* const previousExecutor = currentExecutor;
        const auto* const previousOwner = currentTaskOwner;
        currentExecutor = this;
        currentTaskOwner = &front.first;
        try {
            front.second();
        } catch (const TgBot::TgException& e) {
            LOG(ERROR) << fmt::format(
                "[AsyncConsumer] While handling command: {}: TgApi Exception: "
                "{}",
                front.first, e.what());
        } catch (const std::exception& e) {
            LOG(ERROR) << fmt::format(
                "[AsyncConsumer] While handling command: {}: Exception: {}",
                front.first, e.what());
        } catch (...) {
            LOG(ERROR) << fmt::format(
                "[AsyncConsumer] While handling command: {}: Unknown exception",
                front.first);
        }

        // Destroy the callable before declaring this owner drained. Queued
        // command lambdas retain the module execution lease that protects both
        // invocation and std::function destruction from dlclose.
        front.second = {};
        currentTaskOwner = previousOwner;
        currentExecutor = previousExecutor;

        lock.lock();
        const auto active = activeTasks.find(front.first);
        if (active == activeTasks.end() || active->second == 0) {
            LOG(ERROR) << "Async active-task accounting underflow for "
                       << front.first;
        } else if (--active->second == 0) {
            activeTasks.erase(active);
        }
        lock.unlock();
        drainedVariable.notify_all();
    }
}
