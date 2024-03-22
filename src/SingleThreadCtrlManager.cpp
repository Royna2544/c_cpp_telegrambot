#include <SingleThreadCtrl.h>

#include <mutex>
#include <optional>

#include "Logging.h"

void SingleThreadCtrlManager::destroyController(const ThreadUsage usage, bool deleteIt) {
    static std::array<std::mutex, USAGE_MAX> kPerUsageLocks;
    const std::scoped_lock lk(kPerUsageLocks[usage], mControllerLock);
    auto it = kControllers.find(usage);
    if (it != kControllers.end() && it->second) {
        LOG(LogLevel::VERBOSE, "Stopping: %s controller",
            it->second->mgr_priv.usage.str);
        it->second->stop();
        it->second.reset();
        if (deleteIt)
            kControllers.erase(it);
        LOG(LogLevel::VERBOSE, "Stopped!");
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
    std::unique_lock<std::shared_mutex> lk(mControllerLock);
    std::for_each(kControllers.begin(), kControllers.end(),
                  [this, &lk](const auto& e) {
                      LOG(LogLevel::VERBOSE, "Shutdown: %s controller",
                          e.second->mgr_priv.usage.str);
                      lk.unlock();
                      destroyController(e.first, false);
                      lk.lock();
                      LOG(LogLevel::VERBOSE, "Shutdown done");
                  });
}

