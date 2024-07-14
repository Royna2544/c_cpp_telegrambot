#pragma once

#include <TgBotPPImplExports.h>
#include <absl/log/check.h>
#include <absl/log/log.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "EnumArrayHelpers.h"
#include "InstanceClassBase.hpp"


struct ManagedThread;

class TgBotPPImpl_API ThreadManager : public InstanceClassBase<ThreadManager> {
   public:
    using controller_type = std::shared_ptr<ManagedThread>;

    enum class Usage {
        SOCKET_THREAD,
        SOCKET_EXTERNAL_THREAD,
        TIMER_THREAD,
        SPAMBLOCK_THREAD,
        ERROR_RECOVERY_THREAD,
        IBASH_EXIT_TIMEOUT_THREAD,
        IBASH_UPDATE_OUTPUT_THREAD,
        DATABASE_SYNC_THREAD,
        LOGSERVER_THREAD,
        WEBSERVER_THREAD,
        MAX,
    };

#define USAGE_AND_STR(x) \
    array_helpers::make_elem<Usage, const char*>(Usage::x, #x)
    constexpr static auto ThreadUsageToStrMap =
        array_helpers::make<static_cast<int>(Usage::MAX), Usage, const char*>(
            USAGE_AND_STR(SOCKET_THREAD), USAGE_AND_STR(SOCKET_EXTERNAL_THREAD),
            USAGE_AND_STR(TIMER_THREAD), USAGE_AND_STR(SPAMBLOCK_THREAD),
            USAGE_AND_STR(ERROR_RECOVERY_THREAD),
            USAGE_AND_STR(IBASH_EXIT_TIMEOUT_THREAD),
            USAGE_AND_STR(IBASH_UPDATE_OUTPUT_THREAD),
            USAGE_AND_STR(DATABASE_SYNC_THREAD),
            USAGE_AND_STR(WEBSERVER_THREAD), USAGE_AND_STR(LOGSERVER_THREAD));

    template <Usage u>
    constexpr static const char* ThreadUsageToStr() {
        return array_helpers::find(ThreadUsageToStrMap, u)->second;
    }
    template <Usage usage, class T = ManagedThread, typename... Args>
        requires std::is_base_of_v<ManagedThread, T>
    std::shared_ptr<T> createController(Args... args);

    template <Usage usage, class T = ManagedThread>
        requires std::is_base_of_v<ManagedThread, T>
    std::shared_ptr<T> getController();

    // Stop all controllers managed by this manager, and shutdown this.
    void destroyManager();
    // Destroy a controller given usage
    void destroyController(Usage usage, bool deleteIt = true);

   private:
    std::atomic_bool kIsUnderStopAll = false;
    std::shared_mutex mControllerLock;
    std::unordered_map<Usage, controller_type> kControllers;
};

struct TgBotPPImpl_API ManagedThread {
    using thread_function = std::function<void(void)>;
    using prestop_function = std::function<void(ManagedThread*)>;

    // Set thread function and run - implictly starts the thread as well
    void runWith(thread_function fn);
    // Set the function called before stopping the thread
    void setPreStopFunction(prestop_function fn);
    // Stop the underlying thread
    void stop();
    // Reset the counter, to make this instance reusable
    void reset();
    // Does this controller have a thread inside it?
    [[nodiscard]] bool isRunning() const;

    ManagedThread() {
        timer_mutex.lk = std::unique_lock<std::timed_mutex>(timer_mutex.m);
    };
    virtual ~ManagedThread() {
        if (isRunning()) {
            stop();
        } else if (timer_mutex.lk.owns_lock()) {
            timer_mutex.lk.unlock();
        }
    }

    friend class ThreadManager;

   protected:
    std::atomic_bool kRun = true;
    void delayUnlessStop(const std::chrono::seconds secs) {
        std::unique_lock<std::timed_mutex> lk(timer_mutex.m, std::defer_lock);
        // Unused because of unique_lock dtor
        bool ret [[maybe_unused]] = lk.try_lock_for(secs);
    }
    void delayUnlessStop(const int secs) {
        delayUnlessStop(std::chrono::seconds(secs));
    }

   private:
    enum class ControlState {
        UNINITIALIZED,
        STOPPED_PREMATURE,
        STOPPED_BY_STOP_CMD,
        RUNNING,
    } state = ControlState::UNINITIALIZED;

    void _threadFn(thread_function fn);
    void logInvalidState(const char* state);
    std::optional<std::thread> threadP;
    prestop_function preStop;

    struct {
        // This works, via the main thread will lock the mutex first. Then later
        // thread function would try to lock it, but as it is a timed mutex, it
        // could
        std::timed_mutex m;
        std::unique_lock<std::timed_mutex> lk;
    } timer_mutex;
    struct {
        size_t sizeOfThis;
        struct {
            // It would'nt be a dangling one
            const char* str;
            ThreadManager::Usage val;
        } usage;
    } mgr_priv{};
};

struct TgBotPPImpl_API ManagedThreadRunnable : ManagedThread {
    using ManagedThread::ManagedThread;
    using ManagedThread::runWith;
    virtual void runFunction() = 0;
    void run() {
        ManagedThread::runWith([this] { runFunction(); });
    }
    ~ManagedThreadRunnable() override = default;
};

template <ThreadManager::Usage usage, class T, typename... Args>
    requires std::is_base_of_v<ManagedThread, T>
std::shared_ptr<T> ThreadManager::createController(Args... args) {
    const char* usageStr = ThreadUsageToStr<usage>();
    std::shared_ptr<T> newIt;

    if (getController<usage, T>()) {
        LOG(ERROR) << usageStr << " controller already exists";
        return nullptr;
    }
    std::lock_guard<std::shared_mutex> lock(mControllerLock);

    DLOG(INFO) << "New allocation: " << usageStr << " controller";
    if constexpr (sizeof...(args) != 0) {
        newIt = std::make_shared<T>(std::forward<Args>(args)...);
    } else {
        newIt = std::make_shared<T>();
    }
    auto ctrlit = std::static_pointer_cast<ManagedThread>(newIt);
    ctrlit->mgr_priv.usage.str = usageStr;
    ctrlit->mgr_priv.usage.val = usage;
    ctrlit->mgr_priv.sizeOfThis = sizeof(T);
    CHECK(ctrlit->timer_mutex.lk.owns_lock())
        << usageStr
        << " controller unique_lock is not holding mutex. Probably "
           "constructor is not called.";
    kControllers[usage] = newIt;
    return newIt;
}

template <ThreadManager::Usage usage, class T>
    requires std::is_base_of_v<ManagedThread, T>
std::shared_ptr<T> ThreadManager::getController() {
    const char* usageStr = ThreadUsageToStr<usage>();
    std::lock_guard<std::shared_mutex> lock(mControllerLock);
    auto it = kControllers.find(usage);
    if (it == kControllers.end()) {
        LOG(WARNING) << usageStr << " controller does not exist";
        return nullptr;
    }
    if (it->second->mgr_priv.sizeOfThis != sizeof(T)) {
        LOG(ERROR) << usageStr << " controller is of wrong type";
        return nullptr;
    }
    return std::static_pointer_cast<T>(it->second);
}
