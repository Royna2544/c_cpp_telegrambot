#include <SingleThreadCtrl.h>

#include <mutex>
#include <optional>

#include "InstanceClassBase.hpp"

void SingleThreadCtrlManager::destroyController(const ThreadUsage usage,
                                                bool deleteIt) {
    static std::array<std::mutex, USAGE_MAX> kPerUsageLocks;
    const std::scoped_lock lk(kPerUsageLocks[usage], mControllerLock);
    auto it = kControllers.find(usage);
    if (it != kControllers.end() && it->second) {
        DLOG(INFO) << "Stopping: " << it->second->mgr_priv.usage.str
                   << " controller";
        it->second->stop();
        it->second.reset();
        if (deleteIt) kControllers.erase(it);
        DLOG(INFO) << "Stopped!";
    }
}

std::optional<SingleThreadCtrlManager::controller_type>
SingleThreadCtrlManager::checkRequireFlags(GetControllerFlags opposite,
                                           int flags) {
    if (flags & opposite) {
        if (flags & REQUIRE_FAILACTION_ASSERT)
            LOG(FATAL) << "Flags requested FAILACTION_ASSERT";
        if (flags & REQUIRE_FAILACTION_LOG)
            LOG(ERROR) << "Flags-assertion failed";
        if (flags & REQUIRE_FAILACTION_RETURN_NULL) {
            DLOG(INFO) << "Return null (REQUIRE_FAILACTION_RETURN_NULL)";
            return std::optional(controller_type());
        }
    }
    return std::nullopt;
}
void SingleThreadCtrlManager::destroyManager() {
    std::unique_lock<std::shared_mutex> lk(mControllerLock);
    std::for_each(
        kControllers.begin(), kControllers.end(), [this, &lk](const auto& e) {
            lk.unlock();
            destroyController(e.first, false);
            lk.lock();
        });
}

DECLARE_CLASS_INST(SingleThreadCtrlManager);