#include <fmt/format.h>

#include <ManagedThreads.hpp>
#include <stop_token>

void ManagedThreadRunnable::run() {
    threadP = std::jthread(&ManagedThreadRunnable::threadFunction, this);
}

void ManagedThreadRunnable::threadFunction() {
    ++(*mgr_priv.launched);
    DLOG(INFO) << fmt::format("{} started (lunched: {})", mgr_priv.usage.str,
                              mgr_priv.launched->load());
    auto callback = std::make_unique<StopCallBackJust>(mgr_priv.stopToken,
                                                       [this] { onPreStop(); });
    runFunction(mgr_priv.stopToken);
    if (!mgr_priv.stopToken.stop_requested()) {
        LOG(INFO) << mgr_priv.usage.str << " has ended";
        callback.reset();
    }
    --(*mgr_priv.launched);
    mgr_priv.completeBarrier->count_down();
    DLOG(INFO) << fmt::format("{} joined the barrier (lunched: {})",
                              mgr_priv.usage.str, mgr_priv.launched->load());
}
