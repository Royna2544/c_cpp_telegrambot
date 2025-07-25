#include <fmt/chrono.h>
#include <fmt/format.h>

#include <DurationPoint.hpp>
#include <ManagedThreads.hpp>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <shared_mutex>

#ifdef _POSIX_C_SOURCE
#include <pthread.h>  // pthread_kill

#include <csignal>  // SIGKILL

#define THREAD_FORCE_KILL_SUPPORTED
#endif

#ifdef _WIN32
#include <windows.h>  // TerminateThread

#define THREAD_FORCE_KILL_SUPPORTED
#endif

#ifdef THREAD_FORCE_KILL_SUPPORTED
void kill_thread(std::thread::native_handle_type handle) {
#ifdef _POSIX_C_SOURCE
    pthread_kill(handle, SIGUSR1);
#endif
#ifdef _WIN32
#ifdef _MSC_VER
    TerminateThread(handle, 0);
#endif // _MSC_VER
#endif // _WIN32
}
#endif

constexpr std::chrono::seconds kShutdownDelay(5);

void ThreadManager::destroy() {
    const std::lock_guard<std::shared_mutex> _(mControllerLock);
    DLOG(INFO) << "Starting ThreadManager::destroy, launchCount="
               << launchCount;
    auto countdown = static_cast<int>(Usage::MAX) - launchCount;
    latch.count_down(countdown);
    DLOG(INFO) << "Counted down " << countdown << " times...";
    stopSource.request_stop();
    LOG(INFO) << "Requested stop, now waiting...";

#ifdef THREAD_FORCE_KILL_SUPPORTED
    if (!latch.wait_with_timeout(kShutdownDelay)) {
        LOG(ERROR) << fmt::format(
            "Timed out waiting for threads to finish (waited {})",
            kShutdownDelay);
        for (const auto& thread : kControllers) {
            if (thread.second && thread.second->running()) {
                LOG(ERROR) << fmt::format("Killing thread {}", thread.first);
                kill_thread(thread.second->threadP.native_handle());
            }
        }
    }
#else
    latch.wait();
#endif
}
