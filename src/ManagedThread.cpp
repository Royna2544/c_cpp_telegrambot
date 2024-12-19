#include <fmt/format.h>

#include <ManagedThreads.hpp>
#include <stop_token>

void ThreadRunner::run() {
    threadP = std::jthread(&ThreadRunner::threadFunction, this);
    isRunning = true;
}

int load_before_inc_dec(std::atomic_int* counter) {
    int current = counter->load(std::memory_order_acquire);
    while (!counter->compare_exchange_strong(current, current,
                                             std::memory_order_acquire)) {
        // Reload current until no inc/dec operations interfere
    }
    return current;
}

void ThreadRunner::threadFunction() {
    initLatch.wait();

    ++(*mgr_priv.launched);
    DLOG(INFO) << fmt::format("{} started (launched: {})", mgr_priv.usage,
                              load_before_inc_dec(mgr_priv.launched));
    auto callback = std::make_unique<StopCallBackJust>(mgr_priv.stopToken,
                                                       [this] { onPreStop(); });
    runFunction(mgr_priv.stopToken);
    --(*mgr_priv.launched);
    if (!mgr_priv.stopToken.stop_requested()) {
        LOG(WARNING) << fmt::format("{} has stopped before stop request",
                                    mgr_priv.usage);
        callback.reset();
    } else {
        mgr_priv.completeLatch->count_down();
    }
    DLOG(INFO) << fmt::format("{} joined the barrier (launched: {})",
                              mgr_priv.usage,
                              load_before_inc_dec(mgr_priv.launched));
    isRunning = false;
}
