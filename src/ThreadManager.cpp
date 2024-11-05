#include <fmt/chrono.h>
#include <fmt/format.h>

#include <DurationPoint.hpp>
#include <ManagedThreads.hpp>
#include <mutex>
#include <shared_mutex>

void ThreadManager::destroy() {
    const std::lock_guard<std::shared_mutex> _(mControllerLock);
    DLOG(INFO) << "Starting ThreadManager::destroy, launchCount=" << launchCount;
    auto countdown = static_cast<int>(Usage::MAX) - launchCount;
    barrier.count_down(countdown);
    DLOG(INFO) << "Counted down " << countdown << " times...";
    stopSource.request_stop();
    LOG(INFO) << "Requested stop, now waiting...";
    barrier.wait();
}

constexpr array_helpers::ConstArray<ThreadManager::Usage, const char*,
                                    static_cast<int>(ThreadManager::Usage::MAX)>
    ThreadManager::ThreadUsageToStrMap;  // NOLINT