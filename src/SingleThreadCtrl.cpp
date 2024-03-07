#include <SingleThreadCtrl.h>
#include <mutex>

void SingleThreadCtrl::runWith(thread_function fn) {
    if (!threadP)
        threadP = std::thread(&SingleThreadCtrl::_threadFn, this, fn);
    else {
        LOG_W("Function is already set: %s controller", mgr_priv.usage.str);
    }
}

void SingleThreadCtrl::setPreStopFunction(prestop_function fn) {
    preStop = fn;
}

void SingleThreadCtrl::stop() {
    if (once) {
        if (preStop)
            preStop(this);
        kRun = false;
        if (timer_mutex.lk.owns_lock())
            timer_mutex.lk.unlock();
        if (threadP && threadP->joinable())
            threadP->join();
        once = false;
    };
}

void SingleThreadCtrl::reset() {
    once = true;
    threadP.reset();
    timer_mutex.lk.lock();
}

void SingleThreadCtrl::_threadFn(thread_function fn) {
    fn();
    if (kRun) {
        LOG_I("%s controller ended before stop command", mgr_priv.usage.str);
    }
}