#include <SingleThreadCtrl.h>

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
    tp.Enqueue([this] (const ThreadUsage usage) {
        getController<SingleThreadCtrl>(usage)->stop();
        destroyController(usage);
    }, usage);
}
void SingleThreadCtrlManager::stopAll() {
    for (const auto &[i, j] : kControllers) {
        LOG_V("Shutdown: Controller with usage %d", i);
        j->stop();
    }
}
SingleThreadCtrlManager gSThreadManager;