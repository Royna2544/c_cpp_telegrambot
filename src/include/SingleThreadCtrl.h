#pragma once

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>
#include <type_traits>
#include <unordered_map>

#include "Logging.h"

class SingleThreadCtrl;

class SingleThreadCtrlManager {
 public:
    enum ThreadUsage {
        USAGE_SOCKET_THREAD,
        USAGE_TIMER_THREAD,
        USAGE_SPAMBLOCK_THREAD,
        USAGE_ERROR_RECOVERY_THREAD,
        USAGE_IBASH_TIMEOUT_THREAD,
        USAGE_IBASH_EXIT_TIMEOUT_THREAD,
        USAGE_IBASH_COMMAND_QUEUE_THREAD
    };
    template <class T = SingleThreadCtrl, std::enable_if_t<std::is_base_of_v<SingleThreadCtrl, T>, bool> = true>
    std::shared_ptr<T> getController(const ThreadUsage usage) {
        std::shared_ptr<SingleThreadCtrl> ptr;
        auto it = kControllers.find(usage);

        if (it != kControllers.end()) {
            LOG_V("Using old: Controller with usage %d", usage);
            ptr = it->second;
        } else {
            LOG_V("New allocation: Controller with usage %d", usage);
            ptr = kControllers[usage] = std::make_shared<T>();
        }
        return std::static_pointer_cast<T>(ptr);
    }
    void destroyController(const ThreadUsage usage) {
        auto it = kControllers.find(usage);

        if (it != kControllers.end()) {
            LOG_V("Deleting: Controller with usage %d", usage);
            it->second.reset();
            kControllers.erase(it);
        } else {
            LOG_W("Not allocated: Controller with usage %d", usage);
        }
    }
    void stopAll();
    friend class SingleThreadCtrl;
 private:
   void destroyControllerWithStop(const ThreadUsage usage);
   std::unordered_map<ThreadUsage, std::shared_ptr<SingleThreadCtrl>> kControllers;
};

extern SingleThreadCtrlManager gSThreadManager;

class SingleThreadCtrl {
 public:
    using thread_function = std::function<void(void)>;
    using prestop_function = std::function<void(SingleThreadCtrl *)>;

    SingleThreadCtrl() = default;

    /*
     * setFunction - Assign a thread function and starts the thread
     *
     * @param fn thread function with void(*)() signature
     */
    void setThreadFunction(thread_function fn) {
        if (!threadP)
            threadP = std::thread(&SingleThreadCtrl::_threadFn, this, fn);
        else {
            LOG_W("Function is already set in this instance");
        }
    }

    void setPreStopFunction(prestop_function fn) {
        preStop = fn;
    }

    void stop() {
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

    void reset() {
        once = true;
        threadP.reset();
    }
    
    virtual ~SingleThreadCtrl() {
        stop();
    }

 protected:
    std::atomic_bool kRun = true;
    // Used by std::cv
    std::condition_variable cv;
    bool using_cv = false;
    std::mutex CV_m;
 private:
    void _threadFn(thread_function fn) {
        fn();
        if (kRun) {
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
};

inline void SingleThreadCtrlManager::destroyControllerWithStop(const ThreadUsage usage) {
    
    std::thread([this, usage]{
        getController(usage)->stop();
        destroyController(usage);
    }).detach();
}

inline void SingleThreadCtrlManager::stopAll() {
    for (const auto &[i, j] : kControllers) {
        LOG_V("Shutdown: Controller with usage %d", i);
        j->stop();
    }
}
