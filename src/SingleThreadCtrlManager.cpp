#include <SingleThreadCtrl.h>

#include <latch>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include "Logging.h"


void SingleThreadCtrlManager::destroyController(const ThreadUsage usage) {
    static std::array<std::mutex, USAGE_MAX> kPerUsageLocks;
    const std::scoped_lock lk(kPerUsageLocks[usage], mControllerLock);
    if (!kIsUnderStopAll) {
        auto it = kControllers.find(usage);
        if (it != kControllers.end() && it->second) {
            LOG(LogLevel::VERBOSE, "Stopping: %s controller",
                it->second->mgr_priv.usage.str);
            it->second->stop();
            it->second.reset();
            kControllers.erase(it);
        }
    }
}
std::optional<SingleThreadCtrlManager::controller_type>
SingleThreadCtrlManager::checkRequireFlags(GetControllerFlags opposite,
                                           int flags) {
    if (flags & opposite) {
        if (flags & REQUIRE_FAILACTION_ASSERT)
            ASSERT(false, "Flags requested FAILACTION_ASSERT");
        if (flags & REQUIRE_FAILACTION_LOG)
            LOG(LogLevel::ERROR, "Flags-assertion failed");
        if (flags & REQUIRE_FAILACTION_RETURN_NULL) {
            LOG(LogLevel::VERBOSE,
                "Return null (REQUIRE_FAILACTION_RETURN_NULL)");
            return std::optional(controller_type());
        }
    }
    return std::nullopt;
}
void SingleThreadCtrlManager::destroyManager() {
    std::latch controllersShutdownLH(kControllers.size());
    std::vector<std::thread> threads;
    using ControllerRef = decltype(kControllers)::const_reference;

    kIsUnderStopAll = true;
    std::for_each(kControllers.begin(), kControllers.end(),
                  [&controllersShutdownLH, &threads, this](ControllerRef e) {
                      LOG(LogLevel::VERBOSE, "Shutdown: %s controller",
                            e.second->mgr_priv.usage.str);
                      threads.emplace_back(
                          [e = std::move(e), &controllersShutdownLH, this] {
                              destroyController(e.first);
                              controllersShutdownLH.count_down();
                          });
                  });
    controllersShutdownLH.wait();
    for (auto& i : threads) i.join();
}

SingleThreadCtrlManager gSThreadManager;