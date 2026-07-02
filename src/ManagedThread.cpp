#include <fmt/format.h>

#include <ManagedThreads.hpp>
#include <atomic>
#include <stop_token>

void ThreadRunner::run() {
    threadP = std::jthread(&ThreadRunner::threadFunction, this);
    isRunning = true;
}

void ThreadRunner::threadFunction() {
    initLatch.wait();

    // Relaxed: `launched` is a plain counter (no other memory is published
    // alongside it) - ThreadManager::destroy() reads it under
    // mControllerLock, which already provides the happens-before relationship
    // with kControllers, so the counter itself needs no stronger ordering.
    mgr_priv.launched->fetch_add(1, std::memory_order_relaxed);
    DLOG(INFO) << fmt::format(
        "{} started (launched: {})", mgr_priv.usage,
        mgr_priv.launched->load(std::memory_order_relaxed));
    auto callback = std::make_unique<StopCallBackJust>(mgr_priv.stopToken,
                                                       [this] { onPreStop(); });
    runFunction(mgr_priv.stopToken);
    mgr_priv.launched->fetch_sub(1, std::memory_order_relaxed);
    if (!mgr_priv.stopToken.stop_requested()) {
        LOG(WARNING) << fmt::format("{} has stopped before stop request",
                                    mgr_priv.usage);
        callback.reset();
    } else {
        mgr_priv.completeLatch->count_down();
    }
    DLOG(INFO) << fmt::format(
        "{} joined the barrier (launched: {})", mgr_priv.usage,
        mgr_priv.launched->load(std::memory_order_relaxed));
    isRunning = false;
}
