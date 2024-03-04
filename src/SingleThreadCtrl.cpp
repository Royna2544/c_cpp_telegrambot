#include <SingleThreadCtrl.h>

void SingleThreadCtrl::runWith(thread_function fn) {
    const std::lock_guard<std::mutex> _(ctrl_lk);
    if (!threadP)
        threadP = std::thread(&SingleThreadCtrl::_threadFn, this, fn);
    else {
        LOG_W("Function is already set in this instance");
    }
}

void SingleThreadCtrl::setPreStopFunction(prestop_function fn) {
    const std::lock_guard<std::mutex> _(ctrl_lk);
    preStop = fn;
}

void SingleThreadCtrl::stop() {
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

void SingleThreadCtrl::reset() {
    const std::lock_guard<std::mutex> _(ctrl_lk);
    once = true;
    threadP.reset();
}

void SingleThreadCtrl::_threadFn(thread_function fn) {
    fn();
    if (kRun) {
        const std::lock_guard<std::mutex> _(ctrl_lk);
        const auto& ctrls = gSThreadManager.kControllers;
        LOG_I("A thread ended before stop command, invoke deleter");
        auto it = std::find_if(ctrls.begin(), ctrls.end(),
                               [](std::remove_reference_t<decltype(ctrls)>::const_reference e) {
                                   return std::this_thread::get_id() == e.second->threadP->get_id();
                               });
        if (it != ctrls.end()) {
            gSThreadManager.destroyControllerWithStop(it->first);
        }
    }
}