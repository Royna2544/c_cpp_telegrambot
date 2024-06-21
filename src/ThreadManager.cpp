#include <ManagedThreads.hpp>
#include <mutex>

#include "InstanceClassBase.hpp"

void ThreadManager::destroyController(const Usage usage, bool deleteIt) {
    static std::array<std::mutex, static_cast<int>(Usage::MAX)> kPerUsageLocks;
    const std::scoped_lock lk(kPerUsageLocks[static_cast<int>(usage)], mControllerLock);
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

void ThreadManager::destroyManager() {
    std::unique_lock<std::shared_mutex> lk(mControllerLock);
    std::ranges::for_each(kControllers, [this, &lk](const auto& e) {
        lk.unlock();
        destroyController(e.first, false);
        lk.lock();
    });
}

DECLARE_CLASS_INST(ThreadManager);