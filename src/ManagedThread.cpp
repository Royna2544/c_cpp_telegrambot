#include <fmt/format.h>

#include <ManagedThreads.hpp>
#include <stop_token>

void ManagedThreadRunnable::run() {
    threadP = std::jthread(&ManagedThreadRunnable::threadFunction, this);
}

int load_before_inc_dec(std::atomic_int* counter) {
    int current = counter->load(std::memory_order_acquire);
    while (!counter->compare_exchange_weak(current, current,
                                           std::memory_order_acquire)) {
        // Reload current until no inc/dec operations interfere
    }
    return current;
}

void ManagedThreadRunnable::threadFunction() {
    ++(*mgr_priv.launched);
    DLOG(INFO) << fmt::format("{} started (launched: {})", mgr_priv.usage,
                              load_before_inc_dec(mgr_priv.launched));
    auto callback = std::make_unique<StopCallBackJust>(mgr_priv.stopToken,
                                                       [this] { onPreStop(); });
    runFunction(mgr_priv.stopToken);
    if (!mgr_priv.stopToken.stop_requested()) {
        LOG(INFO) << fmt::format("{} has stopped before stop request",
                                 mgr_priv.usage);
        callback.reset();
    }
    --(*mgr_priv.launched);
    mgr_priv.completeBarrier->count_down();
    DLOG(INFO) << fmt::format("{} joined the barrier (launched: {})",
                              mgr_priv.usage,
                              load_before_inc_dec(mgr_priv.launched));
}
