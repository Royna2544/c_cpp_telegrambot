#pragma once

#include <absl/log/check.h>
#include <absl/log/log.h>
#include <fmt/format.h>

#include <atomic>
#include <chrono>
#include <concepts>
#include <condition_variable>
#include <functional>
#include <latch>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <stop_token>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "trivial_helpers/fruit_inject.hpp"

struct ManagedThreadRunnable;

class ThreadManager {
   public:
    APPLE_INJECT(ThreadManager()) : barrier(static_cast<int>(Usage::MAX)) {}

    enum class Usage {
        SOCKET_THREAD,
        SOCKET_EXTERNAL_THREAD,
        SPAMBLOCK_THREAD,
        LOGSERVER_THREAD,
        WEBSERVER_THREAD,
        MAX
    };

    template <
        std::derived_from<ManagedThreadRunnable> T = ManagedThreadRunnable,
        typename... Args>
        requires std::is_constructible_v<T, Args...>
    T* create(Usage usage, Args&&... args);

    template <
        std::derived_from<ManagedThreadRunnable> T = ManagedThreadRunnable>
    T* get(Usage usage);

    // Stop all controllers managed by this manager, and shutdown this.
    void destroy();

   private:
    std::shared_mutex mControllerLock;
    std::unordered_map<Usage, std::unique_ptr<ManagedThreadRunnable>>
        kControllers;
    std::stop_source stopSource;
    std::latch barrier;
    std::atomic_int launchCount;
};

template <>
struct fmt::formatter<ThreadManager::Usage> : formatter<std::string_view> {
    // parse is inherited from formatter<string_view>.
    auto format(ThreadManager::Usage c,
                format_context& ctx) const -> format_context::iterator {
        string_view name = "unknown";
        switch (c) {
            case ThreadManager::Usage::SOCKET_THREAD:
                name = "SOCKET_THREAD";
                break;
            case ThreadManager::Usage::SOCKET_EXTERNAL_THREAD:
                name = "SOCKET_EXTERNAL_THREAD";
                break;
            case ThreadManager::Usage::SPAMBLOCK_THREAD:
                name = "SPAMBLOCK_THREAD";
                break;
            case ThreadManager::Usage::LOGSERVER_THREAD:
                name = "LOGSERVER_THREAD";
                break;
            case ThreadManager::Usage::WEBSERVER_THREAD:
                name = "WEBSERVER_THREAD";
                break;
            default:
                LOG(ERROR) << "Unknown usage: " << static_cast<int>(c);
                break;
        }
        return formatter<string_view>::format(name, ctx);
    }
};

struct ManagedThreadRunnable {
    using just_function = std::function<void(void)>;
    using StopCallBackJust = std::stop_callback<just_function>;

    // Run the thread
    void run();

    ManagedThreadRunnable() : initLatch(1) {}
    virtual ~ManagedThreadRunnable() = default;

    friend class ThreadManager;

   protected:
    void delayUnlessStop(const std::chrono::seconds secs) {
        std::unique_lock<std::mutex> lk(condvar.mutex);
        condvar.variable.wait_for(
            lk, secs, [this] { return mgr_priv.stopToken.stop_requested(); });
    }

    // The main thread function.
    virtual void runFunction(const std::stop_token& token) = 0;

    // The function called before (or to make it) stop.
    virtual void onPreStop() {}

   private:
    // Underlying thread handle
    std::jthread threadP;
    // Wrapper around 'runFunction'
    void threadFunction();

    struct {
        std::mutex mutex;
        std::condition_variable variable;
    } condvar;

   public:
    struct {
        size_t size{};
        ThreadManager::Usage usage;
        std::stop_token stopToken;
        std::atomic_int* launched;
        std::latch* completeBarrier;
    } mgr_priv{};
    // After filling the privdata, push the latch
    std::latch initLatch;
};

template <std::derived_from<ManagedThreadRunnable> T, typename... Args>
    requires std::is_constructible_v<T, Args...>
T* ThreadManager::create(Usage usage, Args&&... args) {
    std::unique_ptr<T> newIt;

    if (get<T>(usage)) {
        LOG(ERROR) << fmt::format("MGR: {} has already started", usage);
        return nullptr;
    }

    std::lock_guard<std::shared_mutex> lock(mControllerLock);

    LOG(INFO) << fmt::format("MGR: Starting {}...", usage);
    if constexpr (sizeof...(args) != 0) {
        newIt = std::make_unique<T>(std::forward<Args>(args)...);
    } else {
        newIt = std::make_unique<T>();
    }
    newIt->mgr_priv.usage = usage;
    newIt->mgr_priv.size = sizeof(T);
    newIt->mgr_priv.stopToken = stopSource.get_token();
    newIt->mgr_priv.launched = &launchCount;
    newIt->mgr_priv.completeBarrier = &barrier;
    kControllers[usage] = std::move(newIt);
    // Notify privdata init complete
    kControllers[usage]->initLatch.count_down();

    return static_cast<T*>(kControllers[usage].get());
}

template <std::derived_from<ManagedThreadRunnable> T>
T* ThreadManager::get(Usage usage) {
    std::lock_guard<std::shared_mutex> lock(mControllerLock);
    auto it = kControllers.find(usage);
    if (it == kControllers.end()) {
        DLOG(WARNING) << fmt::format("MGR: {} is not created", usage);
        return nullptr;
    }
    if (it->second->mgr_priv.size != sizeof(T)) {
        LOG(ERROR) << fmt::format(
            "MGR: {} wasn't created with size {}, expected {}", usage,
            sizeof(T), it->second->mgr_priv.size);
        return nullptr;
    }
    return static_cast<T*>(it->second.get());
}
