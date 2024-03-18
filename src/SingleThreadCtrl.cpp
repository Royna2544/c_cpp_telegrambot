#include <SingleThreadCtrl.h>

#include <mutex>

#include "Logging.h"

void SingleThreadCtrl::runWith(thread_function fn) {
    if (!threadP)
        threadP = std::thread(&SingleThreadCtrl::_threadFn, this, fn);
    else {
        LOG(LogLevel::WARNING, "Function is already set: %s controller",
            mgr_priv.usage.str);
    }
}

void SingleThreadCtrl::setPreStopFunction(prestop_function fn) { preStop = fn; }

void SingleThreadCtrl::joinThread() {
    if (threadP && threadP->joinable()) threadP->join();
    threadP.reset();
}

void SingleThreadCtrl::stop() {
    if (once) {
        once = false;
        if (preStop) preStop(this);
        kRun = false;
        if (timer_mutex.lk.owns_lock()) timer_mutex.lk.unlock();
        joinThread();
    };
}

void SingleThreadCtrl::reset() {
    if (!once) {
        once = true;
        timer_mutex.lk.lock();
    }
    // In case #reset is called before #stop
    joinThread();
}

void SingleThreadCtrl::_threadFn(thread_function fn) {
    fn();
    if (kRun) {
        LOG(LogLevel::INFO, "%s controller ended before stop command",
            mgr_priv.usage.str);
    }
}