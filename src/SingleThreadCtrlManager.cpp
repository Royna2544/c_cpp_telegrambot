#include <SingleThreadCtrl.h>

#include <chrono>
#include <future>
#include <latch>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <thread>

void SingleThreadCtrlManager::destroyController(const ThreadUsage usage) {
    kShutdownFutures.emplace_back(std::async(std::launch::async, [this, usage] {
        const std::lock_guard<std::shared_mutex> _(mControllerLock);
        _destroyControllerWithoutAsync(usage);
    }));
}

// Caller must hold exclusive mControllerLock
void SingleThreadCtrlManager::_destroyControllerWithoutAsync(const ThreadUsage thisUsage) {
    static std::array<std::mutex, USAGE_MAX> kPerUsageLocks;
    const std::unique_lock<std::mutex> lk(kPerUsageLocks[thisUsage], std::try_to_lock);
    if (lk.owns_lock()) {
        // Do a refind
        auto it = kControllers.find(thisUsage);
        if (it != kControllers.end() && it->second) {
            LOG_V("Stopping: %s controller", ThreadUsageToStr(thisUsage));
            it->second->stop();
            it->second.reset();
        }
    }
}
std::optional<SingleThreadCtrlManager::controller_type>
SingleThreadCtrlManager::checkRequireFlags(GetControllerFlags opposite, int flags) {
    if (flags & opposite) {
        if (flags & FLAG_GETCTRL_REQUIRE_FAILACTION_ASSERT)
            ASSERT(false, "Flags requested FAILACTION_ASSERT");
        if (flags & FLAG_GETCTRL_REQUIRE_FAILACTION_LOG)
            LOG_E("Flags-assertion failed");
        if (flags & FLAG_GETCTRL_REQUIRE_FAILACTION_RETURN_NULL) {
            LOG_V("Return null (FLAG_GETCTRL_REQUIRE_FAILACTION_RETURN_NULL)");
            return std::optional(controller_type());
        }
    }
    return std::nullopt;
}
void SingleThreadCtrlManager::stopAll() {
    std::latch controllersShutdownLH(kControllers.size());
    std::latch futureShutdownLH(kShutdownFutures.size());
    std::vector<std::thread> threads;
    using ControllerRef = decltype(kControllers)::const_reference;
    using FutureRef = std::add_lvalue_reference_t<decltype(kShutdownFutures)::value_type>;

    kIsUnderStopAll = true;
    std::for_each(kControllers.begin(), kControllers.end(),
        [&controllersShutdownLH, &threads, this](ControllerRef e) {
            LOG_V("Shutdown: %s controller", e.second->usageStr);
            threads.emplace_back([e = std::move(e), &controllersShutdownLH, this] {
                const std::lock_guard<std::shared_mutex> _(mControllerLock);
                e.second->stop();
                controllersShutdownLH.count_down();
            });
        }
    );
    controllersShutdownLH.wait();
    for (auto& i : threads)
        i.join();
    threads.clear();

    std::for_each(kShutdownFutures.begin(), kShutdownFutures.end(),
        [&futureShutdownLH, &threads, this](FutureRef e) {
            threads.emplace_back([e = std::move(e), &futureShutdownLH, this] {
                const std::lock_guard<std::shared_mutex> _(mControllerLock);
                e.wait();
                futureShutdownLH.count_down();
            });
        }
    );
    futureShutdownLH.wait();
    for (auto& i : threads)
        i.join();
    kIsUnderStopAll = false;
}
SingleThreadCtrlManager gSThreadManager;