#pragma once

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <future>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <vector>

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
    };

    // Get a controller with usage and flags
    template <class T = SingleThreadCtrl,
              std::enable_if_t<std::is_base_of_v<SingleThreadCtrl, T>, bool> = true>
    std::shared_ptr<T> getController(const ThreadUsage usage, int flags = 0) {
        std::shared_ptr<SingleThreadCtrl> ptr;
        auto it = kControllers.find(usage);

        if (it != kControllers.end()) {
            if (flags & FLAG_GETCTRL_REQUIRE_NONEXIST) {
                if (flags & FLAG_GETCTRL_REQUIRE_NONEXIST_FAILACTION_IGNORE) {
                    LOG_V("Return null");
                    return {};
                }
                checkRequireFlags(flags);
            }
            LOG_V("Using old: Controller with usage %d", usage);
            ptr = it->second;
        } else {
            if (flags & FLAG_GETCTRL_REQUIRE_EXIST)
                checkRequireFlags(flags);
            LOG_V("New allocation: Controller with usage %d", usage);
            ptr = kControllers[usage] = std::make_shared<T>();
        }
        return std::static_pointer_cast<T>(ptr);
    }
    // Destroy a controller given usage
    void destroyController(const ThreadUsage usage);
    // Stop all controllers managed by this manager
    void stopAll();
    friend struct SingleThreadCtrl;

   private:
    static void checkRequireFlags(int flags);
    void destroyControllerWithStop(const ThreadUsage usage);
    std::unordered_map<ThreadUsage, std::shared_ptr<SingleThreadCtrl>> kControllers;
    std::vector<std::future<void>> kShutdownFutures;
};

extern SingleThreadCtrlManager gSThreadManager;

struct SingleThreadCtrl {
    SingleThreadCtrl() = default;
    using thread_function = std::function<void(void)>;
    using prestop_function = std::function<void(SingleThreadCtrl *)>;

    // Set thread function - implictly starts the thread as well
    void setThreadFunction(thread_function fn) {
        const std::lock_guard<std::mutex> _(ctrl_lk);
        if (!threadP)
            threadP = std::thread(&SingleThreadCtrl::_threadFn, this, fn);
        else {
            LOG_W("Function is already set in this instance");
        }
    }

    // Set the function called before stopping the thread
    void setPreStopFunction(prestop_function fn) {
        const std::lock_guard<std::mutex> _(ctrl_lk);
        preStop = fn;
    }

    // Stop the underlying thread
    void stop() {
        const std::lock_guard<std::mutex> _(ctrl_lk);
        if (once) {
            if (preStop)
                preStop(this);
            kRun = false;
            if (using_cv) {
                {
                    std::lock_guard<std::mutex> lk(CV_m);
                }
                cv.notify_one();
            }
            if (threadP && threadP->joinable())
                threadP->join();
            once = false;
        };
    }

    // Reset the counter, to make this instance reusable
    void reset() {
        const std::lock_guard<std::mutex> _(ctrl_lk);
        once = true;
        threadP.reset();
    }

    void allowAutoDelete(const bool allow) {
        const std::lock_guard<std::mutex> _(ctrl_lk);
        kAutoDelete = allow;
    }

    virtual ~SingleThreadCtrl() {
        stop();
    }

    friend class SingleThreadCtrlManager;

   protected:
    std::atomic_bool kRun = true, kAutoDelete = true;
    // Used by std::cv
    std::condition_variable cv;
    bool using_cv = false;
    std::mutex CV_m;

   private:
    void _threadFn(thread_function fn) {
        fn();
        if (kRun && kAutoDelete) {
            const std::lock_guard<std::mutex> _(ctrl_lk);
            const auto& ctrls = gSThreadManager.kControllers;
            LOG_I("A thread ended before stop command");
            auto it = std::find_if(ctrls.begin(), ctrls.end(),
                                   [](std::remove_reference_t<decltype(ctrls)>::const_reference e) {
                                       return std::this_thread::get_id() == e.second->threadP->get_id();
                                   });
            if (it != ctrls.end()) {
                gSThreadManager.destroyControllerWithStop(it->first);
            }
        }
    }

    std::optional<std::thread> threadP;
    prestop_function preStop;
    std::atomic_bool once = true;
    std::mutex ctrl_lk; // To modify the controller, user must hold this lock
};