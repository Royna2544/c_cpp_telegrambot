#include <fmt/chrono.h>
#include <fmt/format.h>

#include <DurationPoint.hpp>
#include <ManagedThreads.hpp>
#include <mutex>

void ThreadManager::destroyController(const Usage usage, bool deleteIt) {
    static std::array<std::mutex, static_cast<int>(Usage::MAX)> kPerUsageLocks;
    const std::scoped_lock lk(kPerUsageLocks[static_cast<int>(usage)],
                              mControllerLock);
    auto it = kControllers.find(usage);
    if (it != kControllers.end() && it->second) {
        LOG(INFO) << "Stopping: " << it->second->mgr_priv.usage.str
                  << " controller";
        DurationPoint dp;
        it->second->stop();
        it->second.reset();
        if (deleteIt) {
            kControllers.erase(it);
        }

        LOG(INFO) << fmt::format("Stopped. Took {}", dp.get());
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
