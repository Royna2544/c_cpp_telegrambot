#include <atomic>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>

#include "Logging.h"

#pragma once

class SingleThreadCtrl {
 public:
    using thread_function = std::function<void(void)>;

    SingleThreadCtrl(thread_function other) {
        setThreadFunction(other);
    }
    SingleThreadCtrl() = default;

    /*
     * setFunction - Assign a thread function and starts the thread
     *
     * @param fn thread function with void(*)() signature
     */
    void setThreadFunction(thread_function fn) {
        if (!threadP)
            threadP = std::thread(fn);
        else {
            LOG_W("Function is already set in this instance");
        }
    }
    void stop() {
        std::call_once(once, [this]{
            kRun = false;
            if (threadP && threadP->joinable())
                threadP->join();
        });
    }

    ~SingleThreadCtrl() {
        stop();
    }
 protected:
    std::atomic_bool kRun = true;
 private:
    std::optional<std::thread> threadP;
    std::once_flag once;
};