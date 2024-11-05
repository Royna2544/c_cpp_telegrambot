#pragma once

#include <TgBotPPImpl_shared_depsExports.h>
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
#include <optional>
#include <shared_mutex>
#include <stop_token>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "EnumArrayHelpers.h"
#include "trivial_helpers/fruit_inject.hpp"

struct ManagedThreadRunnable;

class TgBotPPImpl_shared_deps_API ThreadManager {
   public:
    APPLE_INJECT(ThreadManager()) : barrier(static_cast<int>(Usage::MAX)) {}

    enum class Usage {
        SOCKET_THREAD,
        SOCKET_EXTERNAL_THREAD,
        SPAMBLOCK_THREAD,
        LOGSERVER_THREAD,
        WEBSERVER_THREAD,
        MAX,
    };

#define USAGE_AND_STR(x) \
    array_helpers::make_elem<Usage, const char*>(Usage::x, #x)
    constexpr static auto ThreadUsageToStrMap =
        array_helpers::make<static_cast<int>(Usage::MAX), Usage, const char*>(
            USAGE_AND_STR(SOCKET_THREAD), USAGE_AND_STR(SOCKET_EXTERNAL_THREAD),
            USAGE_AND_STR(SPAMBLOCK_THREAD), USAGE_AND_STR(WEBSERVER_THREAD),
            USAGE_AND_STR(LOGSERVER_THREAD));

    template <Usage u>
    constexpr static std::string_view usageToStr() {
        return array_helpers::find(ThreadUsageToStrMap, u)->second;
    }
    static std::string_view usageToStr(Usage u) {
        return array_helpers::find(ThreadUsageToStrMap, u)->second;
    }

    template <
        Usage usage,
        std::derived_from<ManagedThreadRunnable> T = ManagedThreadRunnable,
        typename... Args>
        requires std::is_constructible_v<T, Args...>
    T* create(Args&&... args) {
        constexpr std::string_view usageStr = usageToStr<usage>();
        return create_internal<T>(usage, usageStr,
                                  std::forward<Args&&>(args)...);
    }
    template <
        std::derived_from<ManagedThreadRunnable> T = ManagedThreadRunnable,
        typename... Args>
        requires std::is_constructible_v<T, Args...>
    T* create(Usage usage, Args&&... args) {
        std::string_view usageStr = usageToStr(usage);
        return create_internal<T>(usage, usageStr,
                                  std::forward<Args&&>(args)...);
    }

    template <Usage usage, std::derived_from<ManagedThreadRunnable> T =
                               ManagedThreadRunnable>
    T* get() {
        constexpr std::string_view usageStr = usageToStr<usage>();
        return get_internal<T>(usage, usageStr);
    }

    template <
        std::derived_from<ManagedThreadRunnable> T = ManagedThreadRunnable>
    T* get(Usage usage) {
        std::string_view usageStr = usageToStr(usage);
        return get_internal<T>(usage, usageStr);
    }

    // Stop all controllers managed by this manager, and shutdown this.
    void destroy();

   private:
    template <class T, typename... Args>
    T* create_internal(Usage usage, std::string_view usageStr, Args&&... args);

    template <class T>
    T* get_internal(Usage usage, std::string_view usageStr);

    std::shared_mutex mControllerLock;
    std::unordered_map<Usage, std::unique_ptr<ManagedThreadRunnable>>
        kControllers;
    std::stop_source stopSource;
    std::latch barrier;
    std::atomic_int launchCount;
};

struct TgBotPPImpl_shared_deps_API ManagedThreadRunnable {
    using just_function = std::function<void(void)>;
    using StopCallBackJust = std::stop_callback<just_function>;

    // Run the thread
    void run();

    ManagedThreadRunnable() = default;
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
        struct {
            // It would'nt be a dangling one
            std::string_view str;
            ThreadManager::Usage val;
        } usage;
        std::stop_token stopToken;
        std::atomic_int* launched;
        std::latch* completeBarrier;
    } mgr_priv{};
};

template <class T, typename... Args>
T* ThreadManager::create_internal(Usage usage, std::string_view usageStr,
                                  Args&&... args) {
    std::unique_ptr<T> newIt;

    if (get<T>(usage)) {
        LOG(ERROR) << usageStr << " controller already exists";
        return nullptr;
    }
    std::lock_guard<std::shared_mutex> lock(mControllerLock);

    DLOG(INFO) << "New allocation: " << usageStr << " controller";
    if constexpr (sizeof...(args) != 0) {
        newIt = std::make_unique<T>(std::forward<Args>(args)...);
    } else {
        newIt = std::make_unique<T>();
    }
    newIt->mgr_priv.usage.str = usageStr;
    newIt->mgr_priv.usage.val = usage;
    newIt->mgr_priv.size = sizeof(T);
    newIt->mgr_priv.stopToken = stopSource.get_token();
    newIt->mgr_priv.launched = &launchCount;
    newIt->mgr_priv.completeBarrier = &barrier;
    kControllers[usage] = std::move(newIt);
    return static_cast<T*>(kControllers[usage].get());
}

template <class T>
T* ThreadManager::get_internal(Usage usage, std::string_view usageStr) {
    std::lock_guard<std::shared_mutex> lock(mControllerLock);
    auto it = kControllers.find(usage);
    if (it == kControllers.end()) {
        DLOG(WARNING) << usageStr << " controller does not exist";
        return nullptr;
    }
    if (it->second->mgr_priv.size != sizeof(T)) {
        LOG(ERROR) << usageStr << " controller is of wrong type";
        return nullptr;
    }
    return static_cast<T*>(it->second.get());
}
