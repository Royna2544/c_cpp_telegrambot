#include <absl/log/log.h>
#include <tgbot/TgException.h>

#include <algorithm>
#include <api/AuthContext.hpp>
#include <api/components/ModuleExecutionContext.hpp>
#include <api/components/ModuleManagement.hpp>
#include <api/components/OnAnyMessage.hpp>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace {

template <typename Callback>
class CancellableCallbackState final
    : public std::enable_shared_from_this<CancellableCallbackState<Callback>> {
    struct ActiveFrame {
        const CancellableCallbackState* state;
        ActiveFrame* previous;
    };

   public:
    class Invocation final {
       public:
        Invocation(std::shared_ptr<CancellableCallbackState> state,
                   const Callback& callback)
            : state_(std::move(state)),
              callback_(callback),
              frame_{.state = state_.get(), .previous = currentFrame_} {
            currentFrame_ = &frame_;
        }

        ~Invocation() {
            // Destroy the per-invocation user closure before advertising that
            // cancellation has drained. It may be the last reference to the
            // callback target.
            callback_ = Callback{};
            currentFrame_ = frame_.previous;
            auto state = std::move(state_);
            state->release();
        }

        Invocation(const Invocation&) = delete;
        Invocation& operator=(const Invocation&) = delete;

        Callback& callback() { return callback_; }

       private:
        std::shared_ptr<CancellableCallbackState> state_;
        Callback callback_;
        ActiveFrame frame_;
    };

    explicit CancellableCallbackState(const Callback& callback)
        : callback_(callback) {}

    [[nodiscard]] std::unique_ptr<Invocation> acquire() {
        std::unique_lock lock(mutex_);
        if (cancelled_ || !callback_) {
            return {};
        }
        ++active_;
        try {
            return std::make_unique<Invocation>(this->shared_from_this(),
                                                callback_);
        } catch (...) {
            --active_;
            drained_.notify_all();
            throw;
        }
    }

    void cancelAndDrain() noexcept {
        try {
            Callback retired;
            {
                std::unique_lock lock(mutex_);
                cancelled_ = true;

                // Synchronous cancellation from inside the callback cannot
                // wait for itself. It still drains any other concurrent
                // invocations; each current Invocation owns a private closure
                // until it returns.
                std::size_t currentAllowance = 0;
                for (auto* frame = currentFrame_; frame != nullptr;
                     frame = frame->previous) {
                    if (frame->state == this) {
                        ++currentAllowance;
                    }
                }
                drained_.wait(lock, [this, currentAllowance] {
                    return active_ <= currentAllowance;
                });
                retired = std::move(callback_);
            }
            // Do not run a user closure's destructor under the state mutex.
            retired = Callback{};
        } catch (const std::exception& error) {
            LOG(ERROR) << "Could not drain callback subscription: "
                       << error.what();
        } catch (...) {
            LOG(ERROR) << "Unknown error while draining callback subscription";
        }
    }

   private:
    void release() {
        {
            const std::lock_guard lock(mutex_);
            if (active_ == 0) {
                LOG(ERROR) << "Callback subscription lease underflow";
                return;
            }
            --active_;
        }
        drained_.notify_all();
    }

    std::mutex mutex_;
    std::condition_variable drained_;
    Callback callback_;
    std::size_t active_ = 0;
    bool cancelled_ = false;

    inline static thread_local ActiveFrame* currentFrame_ = nullptr;
};

template <typename Callback>
class HostCallbackSubscription final : public TgBotApi::CallbackSubscription {
   public:
    explicit HostCallbackSubscription(
        std::shared_ptr<CancellableCallbackState<Callback>> state)
        : state_(std::move(state)) {}

    ~HostCallbackSubscription() override { cancelAndDrain(); }

    void cancelAndDrain() noexcept override {
        auto state = std::exchange(state_, {});
        if (state) {
            state->cancelAndDrain();
        }
    }

   private:
    std::shared_ptr<CancellableCallbackState<Callback>> state_;
};

}  // namespace

namespace tgbot::detail {

class AnyMessageCallbackDispatcher::Impl {
    using EntryId = std::uint64_t;

    struct DrainControl {
        std::mutex mutex;
        std::condition_variable drained;
        std::size_t leases = 0;
    };

    struct CallbackEntry {
        EntryId id = 0;
        std::string ownerCommand;
        TgBotApi::AnyMessageCallback callback;
        std::shared_ptr<DrainControl> drain = std::make_shared<DrainControl>();

        // These fields are protected by Impl::mutex_. A scheduled entry is
        // either present once in ready_ or is currently executing. This is the
        // callback's serial strand.
        std::deque<Message::Ptr> pending;
        bool scheduled = false;
        bool cancelled = false;
    };

    static constexpr std::size_t kMaxCallbacks = 64;

    TgBotApiImpl::Ptr api_;
    const std::size_t maxPendingInvocations_;
    std::mutex mutex_;
    std::condition_variable readyCondition_;
    std::vector<std::shared_ptr<CallbackEntry>> callbacks_;
    std::deque<EntryId> ready_;
    std::size_t pendingInvocations_ = 0;
    EntryId nextId_ = 1;
    bool stopping_ = false;
    std::vector<std::thread> workers_;

    inline static thread_local Impl* currentDispatcher_ = nullptr;
    inline static thread_local EntryId currentEntry_ = 0;

    auto findEntry(EntryId id) {
        return std::ranges::find(callbacks_, id, &CallbackEntry::id);
    }

    void clearPending(const std::shared_ptr<CallbackEntry>& entry) {
        if (entry->pending.size() > pendingInvocations_) {
            LOG(ERROR) << "Any-message pending-count invariant was violated";
            pendingInvocations_ = 0;
        } else {
            pendingInvocations_ -= entry->pending.size();
        }
        entry->pending.clear();
        std::erase(ready_, entry->id);
    }

    static void releaseLease(std::shared_ptr<CallbackEntry> entry) {
        auto drain = entry->drain;

        // A module-owned std::function may have its manager and destructor in
        // the DSO. Drop the worker's last Entry reference before advertising
        // that its lease has drained. removeCallbacksForCommand keeps its own
        // Entry reference until this notification and destroys it before
        // returning to the module loader.
        entry.reset();

        {
            const std::lock_guard lock(drain->mutex);
            if (drain->leases == 0) {
                LOG(ERROR) << "Any-message callback lease underflow";
                return;
            }
            --drain->leases;
        }
        drain->drained.notify_all();
    }

    void workerFunction() {
        while (true) {
            std::shared_ptr<CallbackEntry> entry;
            Message::Ptr message;
            {
                std::unique_lock lock(mutex_);
                readyCondition_.wait(
                    lock, [this] { return stopping_ || !ready_.empty(); });
                if (stopping_) {
                    return;
                }

                const EntryId id = ready_.front();
                ready_.pop_front();
                const auto it = findEntry(id);
                if (it == callbacks_.end() || (*it)->cancelled ||
                    (*it)->pending.empty()) {
                    if (it != callbacks_.end()) {
                        (*it)->scheduled = false;
                    }
                    continue;
                }

                entry = *it;
                message = std::move(entry->pending.front());
                entry->pending.pop_front();
                --pendingInvocations_;

                // Increment while holding the registry mutex. Owner removal
                // either erases the entry first, or observes this lease and
                // waits for it; there is no untracked middle state.
                {
                    const std::lock_guard drainLock(entry->drain->mutex);
                    ++entry->drain->leases;
                }
            }

            const auto previousDispatcher = currentDispatcher_;
            const auto previousEntry = currentEntry_;
            currentDispatcher_ = this;
            currentEntry_ = entry->id;

            bool deregister = false;
            std::shared_ptr<RefLock::SharedLease> moduleLease;
            if (!entry->ownerCommand.empty() && api_ != nullptr &&
                api_->kModuleLoader != nullptr) {
                moduleLease = api_->kModuleLoader->acquireExecutionLease(
                    entry->ownerCommand);
                if (!moduleLease) {
                    // The owning generation is stopping or has already been
                    // replaced. Never enter a DSO callback without a lease.
                    deregister = true;
                }
            }
            try {
                if (!deregister) {
                    module_execution::Scope execution(entry->ownerCommand);
                    deregister = entry->callback(api_, message) ==
                                 TgBotApi::AnyMessageResult::Deregister;
                }
            } catch (const TgBot::TgException& error) {
                LOG(ERROR) << "Telegram error in any-message callback owned by "
                           << entry->ownerCommand << ": " << error.what();
                deregister = true;
            } catch (const std::exception& error) {
                LOG(ERROR) << "Exception in any-message callback owned by "
                           << entry->ownerCommand << ": " << error.what();
                deregister = true;
            } catch (...) {
                LOG(ERROR) << "Unknown exception in any-message callback owned "
                              "by "
                           << entry->ownerCommand;
                deregister = true;
            }

            currentDispatcher_ = previousDispatcher;
            currentEntry_ = previousEntry;
            const EntryId completedId = entry->id;
            bool retire = deregister;

            {
                const std::lock_guard lock(mutex_);
                const auto it = findEntry(completedId);
                if (it != callbacks_.end() && it->get() == entry.get()) {
                    // Direct re-entrant cancellation leaves the current entry
                    // registered-but-cancelled until this lease unwinds. That
                    // keeps it visible to a concurrent module unload.
                    retire = retire || entry->cancelled;
                    if (retire) {
                        entry->cancelled = true;
                        clearPending(entry);
                        entry->scheduled = false;
                    } else if (!entry->pending.empty()) {
                        // Round-robin the strands so one busy callback cannot
                        // monopolize the fixed worker set.
                        ready_.push_back(entry->id);
                    } else {
                        entry->scheduled = false;
                    }
                }
            }
            readyCondition_.notify_all();

            releaseLease(std::move(entry));

            if (retire) {
                // Keep a self-deregistering callback in the registry until its
                // invocation lease is released. Concurrent owner removal then
                // sees and drains it; otherwise destruction completes here,
                // synchronously under the host registry lock.
                const std::lock_guard lock(mutex_);
                const auto it = findEntry(completedId);
                if (it != callbacks_.end() && (*it)->cancelled) {
                    callbacks_.erase(it);
                }
            }
        }
    }

   public:
    Impl(TgBotApiImpl::Ptr api, const std::size_t workerCount,
         const std::size_t maxPendingInvocations)
        : api_(api), maxPendingInvocations_(maxPendingInvocations) {
        if (workerCount == 0 || maxPendingInvocations == 0) {
            throw std::invalid_argument(
                "Any-message dispatcher requires workers and queue capacity");
        }
        try {
            for (std::size_t index = 0; index < workerCount; ++index) {
                workers_.emplace_back([this] { workerFunction(); });
            }
        } catch (...) {
            {
                const std::lock_guard lock(mutex_);
                stopping_ = true;
            }
            readyCondition_.notify_all();
            for (auto& worker : workers_) {
                if (worker.joinable()) {
                    worker.join();
                }
            }
            throw;
        }
    }

    ~Impl() {
        std::vector<std::shared_ptr<CallbackEntry>> removed;
        {
            const std::lock_guard lock(mutex_);
            stopping_ = true;
            for (const auto& entry : callbacks_) {
                entry->cancelled = true;
                entry->pending.clear();
                entry->scheduled = false;
            }
            pendingInvocations_ = 0;
            ready_.clear();
            removed.swap(callbacks_);
        }
        readyCondition_.notify_all();
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        // All workers have dropped their leases. Destroy every callback while
        // the API and any owning module are still alive.
        removed.clear();
    }

    bool registerCallback(std::string ownerCommand,
                          const TgBotApi::AnyMessageCallback& callback) {
        if (!callback) {
            return false;
        }
        const std::lock_guard lock(mutex_);
        if (stopping_ || callbacks_.size() >= kMaxCallbacks ||
            nextId_ == std::numeric_limits<EntryId>::max()) {
            return false;
        }
        auto entry = std::make_shared<CallbackEntry>();
        entry->id = nextId_++;
        entry->ownerCommand = std::move(ownerCommand);
        entry->callback = callback;
        callbacks_.push_back(std::move(entry));
        return true;
    }

    TgBotApi::CallbackSubscription::Ptr subscribeCallback(
        const TgBotApi::AnyMessageCallback& callback) {
        if (!callback) {
            return {};
        }
        using State = CancellableCallbackState<TgBotApi::AnyMessageCallback>;
        auto state = std::make_shared<State>(callback);
        TgBotApi::AnyMessageCallback wrapper =
            [state](TgBotApi::CPtr api, const Message::Ptr& message) {
                auto invocation = state->acquire();
                if (!invocation) {
                    return TgBotApi::AnyMessageResult::Deregister;
                }
                return invocation->callback()(api, message);
            };
        if (!registerCallback({}, wrapper)) {
            state->cancelAndDrain();
            return {};
        }
        return std::make_unique<
            HostCallbackSubscription<TgBotApi::AnyMessageCallback>>(
            std::move(state));
    }

    bool enqueue(Message::Ptr message) {
        std::size_t eligible = 0;
        {
            const std::lock_guard lock(mutex_);
            if (stopping_) {
                return false;
            }
            eligible = static_cast<std::size_t>(std::ranges::count_if(
                callbacks_,
                [](const auto& entry) { return !entry->cancelled; }));
            if (eligible == 0) {
                return true;
            }
            if (pendingInvocations_ > maxPendingInvocations_ ||
                eligible > maxPendingInvocations_ - pendingInvocations_) {
                return false;
            }

            for (const auto& entry : callbacks_) {
                if (entry->cancelled) {
                    continue;
                }
                entry->pending.push_back(message);
                ++pendingInvocations_;
                if (!entry->scheduled) {
                    entry->scheduled = true;
                    ready_.push_back(entry->id);
                }
            }
        }
        readyCondition_.notify_all();
        return true;
    }

    void removeCallbacksForCommand(const std::string_view command) {
        std::vector<std::shared_ptr<CallbackEntry>> removed;
        {
            const std::lock_guard lock(mutex_);
            for (auto it = callbacks_.begin(); it != callbacks_.end();) {
                const auto& entry = *it;
                if (entry->ownerCommand != command) {
                    ++it;
                    continue;
                }
                entry->cancelled = true;
                clearPending(entry);
                entry->scheduled = false;
                if (currentDispatcher_ == this && currentEntry_ == entry->id) {
                    // A callback cannot synchronously drain itself. Leave the
                    // cancelled entry in the registry until worker teardown so
                    // an unload from another thread still finds and drains it.
                    ++it;
                    continue;
                }
                removed.push_back(std::move(*it));
                it = callbacks_.erase(it);
            }
        }
        readyCondition_.notify_all();

        for (const auto& entry : removed) {
            std::unique_lock lock(entry->drain->mutex);
            entry->drain->drained.wait(
                lock, [&entry] { return entry->drain->leases == 0; });
        }

        // Destroy module-owned std::functions before returning to unload.
        removed.clear();
    }
};

AnyMessageCallbackDispatcher::AnyMessageCallbackDispatcher(
    TgBotApiImpl::Ptr api, const std::size_t workerCount,
    const std::size_t maxPendingInvocations)
    : impl_(std::make_unique<Impl>(api, workerCount, maxPendingInvocations)) {}

AnyMessageCallbackDispatcher::~AnyMessageCallbackDispatcher() = default;

bool AnyMessageCallbackDispatcher::registerCallback(
    std::string ownerCommand, const TgBotApi::AnyMessageCallback& callback) {
    return impl_->registerCallback(std::move(ownerCommand), callback);
}

TgBotApi::CallbackSubscription::Ptr
AnyMessageCallbackDispatcher::subscribeCallback(
    const TgBotApi::AnyMessageCallback& callback) {
    return impl_->subscribeCallback(callback);
}

bool AnyMessageCallbackDispatcher::enqueue(Message::Ptr message) {
    return impl_->enqueue(std::move(message));
}

void AnyMessageCallbackDispatcher::removeCallbacksForCommand(
    const std::string_view command) {
    impl_->removeCallbacksForCommand(command);
}

}  // namespace tgbot::detail

void TgBotApiImpl::OnAnyMessageImpl::onAnyMessage(
    const TgBotApi::AnyMessageCallback& callback, std::string ownerCommand) {
    if (!dispatcher_.registerCallback(std::move(ownerCommand), callback)) {
        LOG(ERROR) << "Any-message callback registry is full or stopping";
    }
}

TgBotApi::CallbackSubscription::Ptr
TgBotApiImpl::OnAnyMessageImpl::subscribeAnyMessage(
    const TgBotApi::AnyMessageCallback& callback) {
    auto subscription = dispatcher_.subscribeCallback(callback);
    if (!subscription) {
        LOG(ERROR) << "Any-message callback registry is full or stopping";
    }
    return subscription;
}

TgBotApi::CallbackSubscription::Ptr
TgBotApiImpl::OnAnyMessageImpl::subscribeEditedMessage(
    TgBot::EventBroadcaster::MessageListener listener) {
    if (!listener) {
        return {};
    }
    using Listener = TgBot::EventBroadcaster::MessageListener;
    using State = CancellableCallbackState<Listener>;
    auto state = std::make_shared<State>(listener);
    _api->getEvents().onEditedMessage([state](const Message::Ptr message) {
        try {
            auto invocation = state->acquire();
            if (invocation) {
                invocation->callback()(message);
            }
        } catch (const TgBot::TgException& error) {
            LOG(ERROR) << "Telegram error in edited-message callback: "
                       << error.what();
        } catch (const std::exception& error) {
            LOG(ERROR) << "Exception in edited-message callback: "
                       << error.what();
        } catch (...) {
            LOG(ERROR) << "Unknown exception in edited-message callback";
        }
    });
    return std::make_unique<HostCallbackSubscription<Listener>>(
        std::move(state));
}

void TgBotApiImpl::OnAnyMessageImpl::removeCallbacksForCommand(
    const std::string_view command) {
    dispatcher_.removeCallbacksForCommand(command);
}

void TgBotApiImpl::OnAnyMessageImpl::onAnyMessageFunction(
    Message::Ptr message) {
    if (!dispatcher_.enqueue(std::move(message))) {
        LOG(WARNING) << "Any-message callback queue is full; dropping update";
    }
}

TgBotApiImpl::OnAnyMessageImpl::OnAnyMessageImpl(TgBotApiImpl::Ptr api)
    : _api(api), dispatcher_(api) {
    _api->getEvents().onAnyMessage([this](Message::Ptr message) {
        if (!AuthContext::isUnderTimeLimit(message)) {
            return;
        }
        onAnyMessageFunction(std::move(message));
    });
}
