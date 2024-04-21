#pragma once

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

struct SingleThreadCtrl;

class SingleThreadCtrlManager
    : public InstanceClassBase<SingleThreadCtrlManager> {
   public:
    using controller_type = std::shared_ptr<SingleThreadCtrl>;

    enum GetControllerFlags {
        __REQUIRE_BASE = 1,
        REQUIRE_EXIST = __REQUIRE_BASE,
        REQUIRE_NONEXIST = __REQUIRE_BASE << 1,

        __REQUIRE_FAILACTION_BASE = 1 << 2,
        REQUIRE_FAILACTION_ASSERT = __REQUIRE_FAILACTION_BASE,
        REQUIRE_FAILACTION_LOG = __REQUIRE_FAILACTION_BASE << 1,
        REQUIRE_FAILACTION_RETURN_NULL = __REQUIRE_FAILACTION_BASE << 2,

        SIZEDIFF_ACTION_RECONSTRUCT = __REQUIRE_FAILACTION_BASE << 3,
    };

    enum ThreadUsage {
        USAGE_SOCKET_THREAD,
        USAGE_SOCKET_EXTERNAL_THREAD,
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

    constexpr static auto ThreadUsageToStrMap =
        array_helpers::make<USAGE_MAX, ThreadUsage, const char*>(
            ENUM_AND_STR(USAGE_SOCKET_THREAD),
            ENUM_AND_STR(USAGE_SOCKET_EXTERNAL_THREAD),
            ENUM_AND_STR(USAGE_TIMER_THREAD),
            ENUM_AND_STR(USAGE_SPAMBLOCK_THREAD),
            ENUM_AND_STR(USAGE_ERROR_RECOVERY_THREAD),
            ENUM_AND_STR(USAGE_IBASH_TIMEOUT_THREAD),
            ENUM_AND_STR(USAGE_IBASH_EXIT_TIMEOUT_THREAD),
            ENUM_AND_STR(USAGE_IBASH_COMMAND_QUEUE_THREAD),
            ENUM_AND_STR(USAGE_DATABASE_SYNC_THREAD), ENUM_AND_STR(USAGE_TEST));

    constexpr static const char* ThreadUsageToStr(const ThreadUsage u) {
        return ThreadUsageToStrMap[u].second;
    }

    // Get a controller with usage and flags
    struct GetControllerRequest {
        ThreadUsage usage;
        int flags = 0;
    };

    template <class T = SingleThreadCtrl, typename... Args>
        requires std::is_base_of_v<SingleThreadCtrl, T>
    std::shared_ptr<T> getController(const GetControllerRequest req,
                                     Args... args);

    template <class T = SingleThreadCtrl>
    std::shared_ptr<T> getController(const ThreadUsage usage) {
        return getController<T>({.usage = usage});
    }

    // Stop all controllers managed by this manager, and shutdown this.
    void destroyManager();
    // Destroy a controller given usage
    void destroyController(ThreadUsage usage, bool deleteIt = true);

   private:
    std::atomic_bool kIsUnderStopAll = false;
    static std::optional<controller_type> checkRequireFlags(
        GetControllerFlags opposite, int flags);
    std::shared_mutex mControllerLock;
    std::unordered_map<ThreadUsage, controller_type> kControllers;
};

struct SingleThreadCtrl {
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
    // Does this controller have a thread inside it?
    bool isRunning() const;

    SingleThreadCtrl() {
        timer_mutex.lk = std::unique_lock<std::timed_mutex>(timer_mutex.m);
    };
    virtual ~SingleThreadCtrl() {
        if (timer_mutex.lk.owns_lock()) timer_mutex.lk.unlock();
        stop();
    }

    friend class SingleThreadCtrlManager;

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
            SingleThreadCtrlManager::ThreadUsage val;
        } usage;
    } mgr_priv;
};

struct Empty {};

template <typename T = Empty>
    requires std::is_copy_constructible_v<T>
struct SingleThreadCtrlRunnable : SingleThreadCtrl {
    using SingleThreadCtrl::runWith;
    using SingleThreadCtrl::SingleThreadCtrl;
    virtual void runFunction() = 0;
    void run() {
        SingleThreadCtrl::runWith(
            std::bind(&SingleThreadCtrlRunnable::runFunction, this));
    }
    void setPriv(const std::shared_ptr<T> _priv) { priv = _priv; }
    virtual ~SingleThreadCtrlRunnable() {}

   protected:
    std::shared_ptr<T> priv;
};

template <class T, typename... Args>
    requires std::is_base_of_v<SingleThreadCtrl, T>
std::shared_ptr<T> SingleThreadCtrlManager::getController(
    const GetControllerRequest req, Args... args) {
    controller_type ptr;
    bool sizeMismatch = false;
    const char* usageStr = ThreadUsageToStr(req.usage);
    std::unique_lock<std::shared_mutex> lk(mControllerLock);
    auto it = kControllers.find(req.usage);

    if (kIsUnderStopAll) {
        LOG(WARNING) << "Under stopAll(), ignore";
        return {};
    }
    if (sizeMismatch = it != kControllers.end() &&
                       it->second->mgr_priv.sizeOfThis < sizeof(T);
        sizeMismatch) {
        LOG(WARNING) << "Size mismatch: Buffer has "
                     << it->second->mgr_priv.sizeOfThis << ", New class wants "
                     << sizeof(T);
        if (!(req.flags & SIZEDIFF_ACTION_RECONSTRUCT)) return {};
    }
    if (it != kControllers.end() && it->second && !sizeMismatch) {
        if (const auto maybeRet =
                checkRequireFlags(REQUIRE_NONEXIST, req.flags);
            maybeRet)
            ptr = maybeRet.value();
        else {
            DLOG(INFO) << "Using old: " << usageStr << " controller";
            ptr = it->second;
        }
    } else {
        if (const auto maybeRet = checkRequireFlags(REQUIRE_EXIST, req.flags);
            maybeRet && !sizeMismatch)
            ptr = maybeRet.value();
        else {
            std::shared_ptr<T> newit;
            if (sizeMismatch) {
                lk.unlock();
                destroyController(it->first);
                lk.lock();
            }
            DLOG(INFO) << "New allocation: " << usageStr << " controller";
            if constexpr (sizeof...(args) != 0)
                newit = std::make_shared<T>(std::forward<Args>(args)...);
            else
                newit = std::make_shared<T>();
            auto ctrlit = std::static_pointer_cast<SingleThreadCtrl>(newit);
            ctrlit->mgr_priv.usage.str = usageStr;
            ctrlit->mgr_priv.usage.val = req.usage;
            ctrlit->mgr_priv.sizeOfThis = sizeof(T);
            LOG_IF(FATAL, !ctrlit->timer_mutex.lk.owns_lock())
                << usageStr
                << " controller unique_lock is not holding mutex. Probably "
                   "constructor is not called.";
            ptr = kControllers[req.usage] = newit;
        }
    }
    return std::static_pointer_cast<T>(ptr);
}
