#include <SingleThreadCtrl.h>
#include <mutex>
#include <shared_mutex>

void SingleThreadCtrl::runWith(thread_function fn) {
    if (!threadP)
        threadP = std::thread(&SingleThreadCtrl::_threadFn, this, fn);
    else {
        LOG_W("Function is already set: %s controller", usageStr);
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
        _TimerLk.unlock();
        if (threadP && threadP->joinable())
            threadP->join();
        once = false;
    };
}

void SingleThreadCtrl::reset() {
    once = true;
    threadP.reset();
    _TimerLk.lock();
}

void SingleThreadCtrl::_threadFn(thread_function fn) {
    fn();
    if (kRun) {
        LOG_I("%s controller ended before stop command", usageStr);
    }
    if (!gSThreadManager.kIsUnderStopAll) {
        const std::shared_lock<std::shared_mutex> lk(gSThreadManager.mControllerLock);
        auto& ctrls = gSThreadManager.kControllers;

        auto it = std::find_if(ctrls.begin(), ctrls.end(),
                    [](std::remove_reference_t<decltype(ctrls)>::const_reference e) {
                        return std::this_thread::get_id() == e.second->threadP->get_id();
                    });
        if (it != ctrls.end()) {
            gSThreadManager.destroyController(it->first);
        }
    }
}