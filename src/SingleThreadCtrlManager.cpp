#include <SingleThreadCtrl.h>
#include <future>
#include <latch>

void SingleThreadCtrlManager::destroyController(const ThreadUsage usage) {
    auto it = kControllers.find(usage);

    if (it != kControllers.end()) {
        {
            const std::lock_guard<std::mutex> _(it->second->ctrl_lk);
        }
        LOG_V("Deleting: Controller with usage %d", usage);
        it->second.reset();
        kControllers.erase(it);
    } else {
        LOG_W("Not allocated: Controller with usage %d", usage);
    }
}
void SingleThreadCtrlManager::checkRequireFlags(int flags) {
    if (flags & FLAG_GETCTRL_REQUIRE_FAILACTION_ASSERT)
        ASSERT(false, "Flags requested FAILACTION_ASSERT");
    if (flags & FLAG_GETCTRL_REQUIRE_FAILACTION_LOG)
        LOG_E("Flags-assertion failed");
}
void SingleThreadCtrlManager::destroyControllerWithStop(const ThreadUsage usage) {
    kShutdownFutures.emplace_back(std::async(std::launch::async, [this, usage] {
        getController<SingleThreadCtrl>(usage)->stop();
        destroyController(usage);
    }));
}
void SingleThreadCtrlManager::stopAll() {
    std::latch controllersShutdownLH(kControllers.size());
    std::latch futureShutdownLH(kShutdownFutures.size());
    std::vector<std::thread> threads;
    using ControllerRef = decltype(kControllers)::const_reference;
    using FutureRef = std::add_lvalue_reference_t<decltype(kShutdownFutures)::value_type>;

    std::for_each(kControllers.begin(), kControllers.end(), 
        [&controllersShutdownLH, &threads](ControllerRef e) {
        LOG_V("Shutdown: Controller with usage %d", e.first);
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
}
SingleThreadCtrlManager gSThreadManager;