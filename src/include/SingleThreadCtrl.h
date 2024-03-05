#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "EnumArrayHelpers.h"
#include "Logging.h"

struct SingleThreadCtrl;

class SingleThreadCtrlManager {
   public:
    using controller_type = std::shared_ptr<SingleThreadCtrl>;

    enum GetControllerFlags {
        __FLAG_GETCTRL_REQUIRE_BASE = 1,
        FLAG_GETCTRL_REQUIRE_EXIST = __FLAG_GETCTRL_REQUIRE_BASE,
        FLAG_GETCTRL_REQUIRE_NONEXIST = __FLAG_GETCTRL_REQUIRE_BASE << 1,

        __FLAG_GETCTRL_REQUIRE_FAILACTION_BASE = 1 << 2,
        FLAG_GETCTRL_REQUIRE_FAILACTION_ASSERT = __FLAG_GETCTRL_REQUIRE_FAILACTION_BASE,
        FLAG_GETCTRL_REQUIRE_FAILACTION_LOG = __FLAG_GETCTRL_REQUIRE_FAILACTION_BASE << 1,
        FLAG_GETCTRL_REQUIRE_FAILACTION_RETURN_NULL = __FLAG_GETCTRL_REQUIRE_FAILACTION_BASE << 2
    };

    enum ThreadUsage {
        USAGE_SOCKET_THREAD,
        USAGE_TIMER_THREAD,
        USAGE_SPAMBLOCK_THREAD,
        USAGE_ERROR_RECOVERY_THREAD,
        USAGE_IBASH_TIMEOUT_THREAD,
        USAGE_IBASH_EXIT_TIMEOUT_THREAD,
        USAGE_IBASH_COMMAND_QUEUE_THREAD,
        USAGE_DATABASE_SYNC_THREAD,
        USAGE_TEST,
        USAGE_MAX,
    };

    constexpr static auto ThreadUsageToStrMap = array_helpers::make<USAGE_MAX, ThreadUsage, const char*> (
        ENUM_AND_STR(USAGE_SOCKET_THREAD),
        ENUM_AND_STR(USAGE_TIMER_THREAD),
        ENUM_AND_STR(USAGE_SPAMBLOCK_THREAD),
        ENUM_AND_STR(USAGE_ERROR_RECOVERY_THREAD),
        ENUM_AND_STR(USAGE_IBASH_TIMEOUT_THREAD),
        ENUM_AND_STR(USAGE_IBASH_EXIT_TIMEOUT_THREAD),
        ENUM_AND_STR(USAGE_IBASH_COMMAND_QUEUE_THREAD),
        ENUM_AND_STR(USAGE_DATABASE_SYNC_THREAD),
        ENUM_AND_STR(USAGE_TEST)
    );

    constexpr static const char* ThreadUsageToStr(const ThreadUsage u) {
        return ThreadUsageToStrMap[u].second;
    }

    // Get a controller with usage and flags
    template <class T = SingleThreadCtrl,
              std::enable_if_t<std::is_base_of_v<SingleThreadCtrl, T>, bool> = true>
    std::shared_ptr<T> getController(const ThreadUsage usage, int flags = 0);
    // Stop all controllers managed by this manager
    void stopAll();
    friend struct SingleThreadCtrl;
#ifdef _SINGLETHREADCTRL_TEST
    friend struct SingleThreadCtrlTestAccessors;
#endif

   private:
    std::atomic_bool kIsUnderStopAll = false;
    static std::optional<controller_type> checkRequireFlags(GetControllerFlags opposite, int flags);
    std::shared_mutex mControllerLock;
    std::unordered_map<ThreadUsage, controller_type> kControllers;
    std::vector<std::future<void>> kShutdownFutures;
    // Destroy a controller given usage
    void destroyController(ThreadUsage usage);
};

extern SingleThreadCtrlManager gSThreadManager;

struct SingleThreadCtrl {
    SingleThreadCtrl() {
        _TimerLk = std::unique_lock<std::timed_mutex>(timer_lock);
    };
    using thread_function = std::function<void(void)>;
    using prestop_function = std::function<void(SingleThreadCtrl*)>;

    // Set thread function and run - implictly starts the thread as well
    void runWith(thread_function fn);
    // Set the function called before stopping the thread
    void setPreStopFunction(prestop_function fn);
    // Stop the underlying thread
    void stop();
    // Reset the counter, to make this instance reusable
    void reset();

    virtual ~SingleThreadCtrl() {
        stop();
    }

    friend class SingleThreadCtrlManager;

   protected:
    std::atomic_bool kRun = true;
    void delayUnlessStop(const std::chrono::seconds secs) {
        std::unique_lock<std::timed_mutex> lk(timer_lock, std::defer_lock);
        lk.try_lock_for(secs);
    }
    void delayUnlessStop(const int secs) {
        delayUnlessStop(std::chrono::seconds(secs));
    }

   private:
    void _threadFn(thread_function fn);
    // It would'nt be a dangling one
    const char* usageStr;
    std::optional<std::thread> threadP;
    prestop_function preStop;
    std::atomic_bool once = true;
    // This works, via the main thread will lock the mutex first. Then later thread function
    // would try to lock it, but as it is a timed mutex, it could 
    std::timed_mutex timer_lock;
    std::unique_lock<std::timed_mutex> _TimerLk;
};

struct SingleThreadCtrlRunnable : SingleThreadCtrl {
    using SingleThreadCtrl::runWith;
    using SingleThreadCtrl::SingleThreadCtrl;
    virtual void runFunction() = 0;
    void run() {
        SingleThreadCtrl::runWith(std::bind(&SingleThreadCtrlRunnable::runFunction, this));
    }
    virtual ~SingleThreadCtrlRunnable() {}
};

template <class T, std::enable_if_t<std::is_base_of_v<SingleThreadCtrl, T>, bool>>
std::shared_ptr<T> SingleThreadCtrlManager::getController(const ThreadUsage usage, int flags) {
    controller_type ptr;
    const std::lock_guard<std::shared_mutex> _(mControllerLock);
    auto it = kControllers.find(usage);

#ifndef _SINGLETHREADCTRL_TEST
    ASSERT(usage != USAGE_TEST, "USAGE_TEST used in main program");
#endif

    if (kIsUnderStopAll) {
        LOG_W("Under stopAll(), ignore");
        return {};
    }
    if (it != kControllers.end()) {
        if (const auto maybeRet = checkRequireFlags(FLAG_GETCTRL_REQUIRE_NONEXIST, flags); maybeRet)
            ptr = maybeRet.value();
        else {
            LOG_V("Using old: %s controller", ThreadUsageToStr(usage));
            ptr = it->second;
        }
    } else {
        if (const auto maybeRet = checkRequireFlags(FLAG_GETCTRL_REQUIRE_EXIST, flags); maybeRet)
            ptr = maybeRet.value();
        else {
            LOG_V("New allocation: %s controller", ThreadUsageToStr(usage));
            auto newit = std::make_shared<T>();
            std::static_pointer_cast<SingleThreadCtrl>(newit)->usageStr = ThreadUsageToStr(usage);
            ptr = kControllers[usage] = newit; 
        }
    }
    return std::static_pointer_cast<T>(ptr);
}