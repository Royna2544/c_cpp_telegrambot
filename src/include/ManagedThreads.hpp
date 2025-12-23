#pragma once

#include <AbslLogCompat.hpp>
#include <AbslLogCompat.hpp>
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

struct ThreadRunner;

class LatchWithTimeout {
   public:
    explicit LatchWithTimeout(std::ptrdiff_t count) : latch(count) {}

    void count_down(std::ptrdiff_t update = 1) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            latch.count_down(update);
        }
        cv.notify_all();
    }

    bool wait_with_timeout(std::chrono::seconds timeout) {
        std::unique_lock<std::mutex> lock(mtx);
        return cv.wait_for(lock, timeout, [this] { return latch.try_wait(); });
    }

    void wait() { latch.wait(); }

   private:
    std::latch latch;
    std::condition_variable cv;
    std::mutex mtx;
};

class ThreadManager {
   public:
    APPLE_INJECT(ThreadManager()) : latch(static_cast<int>(Usage::MAX)) {}

    enum class Usage {
        SOCKET_THREAD,
        SOCKET_EXTERNAL_THREAD,
        SPAMBLOCK_THREAD,
        LOGSERVER_THREAD,
        WEBSERVER_THREAD,
        MAX
    };

    template <std::derived_from<ThreadRunner> T = ThreadRunner,
              typename... Args>
        requires std::is_constructible_v<T, Args...>
    T* create(Usage usage, Args&&... args);

    template <std::derived_from<ThreadRunner> T = ThreadRunner>
    T* get(Usage usage);

    // Stop all controllers managed by this manager, and shutdown this.
    void destroy();

   private:
    std::shared_mutex mControllerLock;
    std::unordered_map<Usage, std::unique_ptr<ThreadRunner>> kControllers;
    std::stop_source stopSource;
    LatchWithTimeout latch;
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

struct ThreadRunner {
    using just_function = std::function<void(void)>;
    using StopCallBackJust = std::stop_callback<just_function>;

    // Run the thread
    void run();

    ThreadRunner() : initLatch(1) {}
    virtual ~ThreadRunner() = default;

    friend class ThreadManager;

   protected:
    // The main thread function.
    virtual void runFunction(const std::stop_token& token) = 0;

    // The function called before (or to make it) stop.
    virtual void onPreStop() {}

   private:
    // Underlying thread handle
    std::jthread threadP;
    // Wrapper around 'runFunction'
    void threadFunction();

    bool isRunning = false;

   public:
    struct {
        size_t size{};
        ThreadManager::Usage usage;
        std::stop_token stopToken;
        std::atomic_int* launched;
        LatchWithTimeout* completeLatch;
    } mgr_priv{};
    // After filling the privdata, push the latch
    std::latch initLatch;

    [[nodiscard]] bool running() const noexcept { return isRunning; }
};

template <std::derived_from<ThreadRunner> T, typename... Args>
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
    newIt->mgr_priv.completeLatch = &latch;
    kControllers[usage] = std::move(newIt);
    // Notify privdata init complete
    kControllers[usage]->initLatch.count_down();

    return static_cast<T*>(kControllers[usage].get());
}

template <std::derived_from<ThreadRunner> T>
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
