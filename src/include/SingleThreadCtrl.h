#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "EnumArrayHelpers.h"
#include "Logging.h"

struct SingleThreadCtrl;

class SingleThreadCtrlManager {
   public:
    constexpr static int GetControllerActionShift = 15;

    enum GetControllerFlags {
        FLAG_GETCTRL_REQUIRE_EXIST = 1 << 0,
        FLAG_GETCTRL_REQUIRE_NONEXIST = 1 << 1,
        FLAG_GETCTRL_REQUIRE_FAILACTION_ASSERT = 1 << GetControllerActionShift,
        FLAG_GETCTRL_REQUIRE_FAILACTION_LOG = 1 << (GetControllerActionShift + 1),
        FLAG_GETCTRL_REQUIRE_NONEXIST_FAILACTION_IGNORE = 1 << (GetControllerActionShift + 2)
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
#ifdef _SINGLETHREADCTRL_TEST
        USAGE_TEST,
#endif
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
#ifdef _SINGLETHREADCTRL_TEST
        ENUM_AND_STR(USAGE_DATABASE_SYNC_THREAD),
        ENUM_AND_STR(USAGE_TEST)
#else
        ENUM_AND_STR(USAGE_DATABASE_SYNC_THREAD)
#endif
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
    static void checkRequireFlags(int flags);
    std::unordered_map<ThreadUsage, std::shared_ptr<SingleThreadCtrl>> kControllers;
    std::vector<std::future<void>> kShutdownFutures;
    // Destroy a controller given usage
    void destroyController(decltype(kControllers)::iterator it);
    void destroyController(ThreadUsage usage);
};

extern SingleThreadCtrlManager gSThreadManager;

struct SingleThreadCtrl {
    SingleThreadCtrl() = default;
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
    // Used by std::cv
    std::condition_variable cv;
    bool using_cv = false;
    std::mutex CV_m;

   private:
    void _threadFn(thread_function fn);
    // It would'nt be a dangling one
    const char* usageStr;
    std::optional<std::thread> threadP;
    prestop_function preStop;
    std::atomic_bool once = true;
    std::mutex ctrl_lk;  // To modify the controller, user must hold this lock
};

struct SingleThreadCtrlRunnable : SingleThreadCtrl {
    virtual void runFunction() = 0;
    void run() {
        SingleThreadCtrl::runWith(std::bind(&SingleThreadCtrlRunnable::runFunction, this));
    }
    using SingleThreadCtrl::runWith;
    using SingleThreadCtrl::SingleThreadCtrl;
    virtual ~SingleThreadCtrlRunnable() {}
};

template <class T, std::enable_if_t<std::is_base_of_v<SingleThreadCtrl, T>, bool>>
std::shared_ptr<T> SingleThreadCtrlManager::getController(const ThreadUsage usage, int flags) {
    std::shared_ptr<SingleThreadCtrl> ptr;
    auto it = kControllers.find(usage);

    if (kIsUnderStopAll) {
        LOG_W("Under stopAll(), ignore");
        return {};
    }
    if (it != kControllers.end()) {
        if (flags & FLAG_GETCTRL_REQUIRE_NONEXIST) {
            if (flags & FLAG_GETCTRL_REQUIRE_NONEXIST_FAILACTION_IGNORE) {
                LOG_V("Return null (FLAG_GETCTRL_REQUIRE_NONEXIST_FAILACTION_IGNORE)");
                return {};
            }
            checkRequireFlags(flags);
        }
        LOG_V("Using old: %s controller", SingleThreadCtrlManager::ThreadUsageToStr(usage));
        ptr = it->second;
    } else {
        if (flags & FLAG_GETCTRL_REQUIRE_EXIST)
            checkRequireFlags(flags);
        LOG_V("New allocation: %s controller", SingleThreadCtrlManager::ThreadUsageToStr(usage));
        auto newit = std::make_shared<T>();
        std::static_pointer_cast<SingleThreadCtrl>(newit)->usageStr = ThreadUsageToStr(usage);
        ptr = kControllers[usage] = newit; 
    }
    return std::static_pointer_cast<T>(ptr);
}