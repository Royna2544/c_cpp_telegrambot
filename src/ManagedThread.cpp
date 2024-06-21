#include <ManagedThreads.hpp>
#include <mutex>

bool ManagedThread::isRunning() const { return state == ControlState::RUNNING; }

void ManagedThread::logInvalidState(const char *func) {
    LOG(ERROR) << "Invalid state " << static_cast<int>(state) << " for " << func
               << ": " << mgr_priv.usage.str << " controller";
}

void ManagedThread::runWith(thread_function fn) {
    switch (state) {
        case ControlState::STOPPED_PREMATURE:
        case ControlState::STOPPED_BY_STOP_CMD:
            reset();
            [[fallthrough]];
        case ControlState::UNINITIALIZED:
            threadP = std::thread(&ManagedThread::_threadFn, this, fn);
            state = ControlState::RUNNING;
            break;
        case ControlState::RUNNING:
            LOG(WARNING) << "Thread is already running: " << mgr_priv.usage.str
                         << " controller";
            break;
    };
}

void ManagedThread::setPreStopFunction(prestop_function fn) {
    switch (state) {
        case ControlState::UNINITIALIZED:
        case ControlState::STOPPED_BY_STOP_CMD:
        case ControlState::STOPPED_PREMATURE:
        case ControlState::RUNNING:
            preStop = fn;
            break;
    };
}

void ManagedThread::stop() {
    switch (state) {
        case ControlState::RUNNING:
            if (preStop) {
                preStop(this);
            }
            kRun = false;
            timer_mutex.lk.unlock();
            state = ControlState::STOPPED_BY_STOP_CMD;
            [[fallthrough]];
        case ControlState::STOPPED_PREMATURE:
            if (threadP->joinable()) threadP->join();
            threadP.reset();
            break;
        default:
            break;
    }
}

void ManagedThread::reset() {
    switch (state) {
        case ControlState::RUNNING:
        case ControlState::STOPPED_PREMATURE:
            stop();
            [[fallthrough]];
        case ControlState::STOPPED_BY_STOP_CMD:
            kRun = true;
            timer_mutex.lk.lock();
            state = ControlState::UNINITIALIZED;
            break;
        default:
            logInvalidState(__func__);
            break;
    }
}

void ManagedThread::_threadFn(thread_function fn) {
    fn();
    if (kRun) {
        LOG(INFO) << mgr_priv.usage.str
                  << " controller ended before stop command";
        state = ControlState::STOPPED_PREMATURE;
        kRun = false;
    }
}