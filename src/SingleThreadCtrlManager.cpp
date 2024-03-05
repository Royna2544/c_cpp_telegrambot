#include <SingleThreadCtrl.h>

#include <future>
#include <latch>
#include <optional>

void SingleThreadCtrlManager::destroyController(decltype(kControllers)::iterator it) {
    ASSERT(it->second, "Controller with type %d is null, but deletion requested", it->first);
    kShutdownFutures.emplace_back(std::async(std::launch::async, [this, it] {
        it->second->stop();
        LOG_V("Deleting: %s controller", SingleThreadCtrlManager::ThreadUsageToStr(it->first));
        {
            const std::lock_guard<std::mutex> _(it->second->ctrl_lk);
        }
        it->second.reset();
        kControllers.erase(it);
    }));
}
void SingleThreadCtrlManager::destroyController(ThreadUsage usage) {
    auto it = kControllers.find(usage);
    if (it != kControllers.end()) {
        destroyController(it);
    } else {
        LOG_W("Couldn't find %s controller to delete", it->second->usageStr);
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
            return {};
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
                  [&controllersShutdownLH, &threads](ControllerRef e) {
                      LOG_V("Shutdown: %s controller", e.second->usageStr);
                      threads.emplace_back([e = std::move(e), &controllersShutdownLH] {
                          e.second->stop();
                          controllersShutdownLH.count_down();
                      });
                  });
    controllersShutdownLH.wait();
    for (auto& i : threads)
        i.join();
    threads.clear();

    std::for_each(kShutdownFutures.begin(), kShutdownFutures.end(),
                  [&futureShutdownLH, &threads](FutureRef e) {
                      threads.emplace_back([e = std::move(e), &futureShutdownLH]() {
                          e.wait();
                          futureShutdownLH.count_down();
                      });
                  });
    futureShutdownLH.wait();
    for (auto& i : threads)
        i.join();
    kIsUnderStopAll = false;
}
SingleThreadCtrlManager gSThreadManager;