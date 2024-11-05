#include <fmt/format.h>

#include <ManagedThreads.hpp>
#include <stop_token>

void ManagedThreadRunnable::run() {
    threadP = std::jthread(&ManagedThreadRunnable::threadFunction, this);
}

void ManagedThreadRunnable::threadFunction(const std::stop_token& token,
                                           ManagedThreadRunnable* thiz) {
    ++(*thiz->mgr_priv.launched);
    DLOG(INFO) << fmt::format("{} started (lunched: {})",
                              thiz->mgr_priv.usage.str,
                              thiz->mgr_priv.launched->load());
    auto callback = std::make_unique<StopCallBackJust>(
        thiz->mgr_priv.stopToken, [thiz] { thiz->onPreStop(); });
    thiz->runFunction(token);
    if (!thiz->mgr_priv.stopToken.stop_requested()) {
        LOG(INFO) << thiz->mgr_priv.usage.str << " has ended";
        callback.reset();
    }
    --(*thiz->mgr_priv.launched);
    thiz->mgr_priv.completeBarrier->count_down();
    DLOG(INFO) << fmt::format("{} joined the barrier (lunched: {})",
                              thiz->mgr_priv.usage.str,
                              thiz->mgr_priv.launched->load());
}
