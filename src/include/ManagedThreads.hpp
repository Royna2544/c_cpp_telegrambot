#pragma once

#include <TgBotPPImpl_shared_depsExports.h>
#include <absl/log/check.h>
#include <absl/log/log.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "EnumArrayHelpers.h"
#include "trivial_helpers/fruit_inject.hpp"

struct ManagedThread;

class TgBotPPImpl_shared_deps_API ThreadManager {
   public:
    APPLE_INJECT(ThreadManager()) = default;

    enum class Usage {
        SOCKET_THREAD,
        SOCKET_EXTERNAL_THREAD,
        TIMER_THREAD,
        SPAMBLOCK_THREAD,
        ERROR_RECOVERY_THREAD,
        IBASH_EXIT_TIMEOUT_THREAD,
        IBASH_UPDATE_OUTPUT_THREAD,
        DATABASE_SYNC_THREAD,
        LOGSERVER_THREAD,
        WEBSERVER_THREAD,
        MAX,
    };

#define USAGE_AND_STR(x) \
    array_helpers::make_elem<Usage, const char*>(Usage::x, #x)
    constexpr static auto ThreadUsageToStrMap =
        array_helpers::make<static_cast<int>(Usage::MAX), Usage, const char*>(
            USAGE_AND_STR(SOCKET_THREAD), USAGE_AND_STR(SOCKET_EXTERNAL_THREAD),
            USAGE_AND_STR(TIMER_THREAD), USAGE_AND_STR(SPAMBLOCK_THREAD),
            USAGE_AND_STR(ERROR_RECOVERY_THREAD),
            USAGE_AND_STR(IBASH_EXIT_TIMEOUT_THREAD),
            USAGE_AND_STR(IBASH_UPDATE_OUTPUT_THREAD),
            USAGE_AND_STR(DATABASE_SYNC_THREAD),
            USAGE_AND_STR(WEBSERVER_THREAD), USAGE_AND_STR(LOGSERVER_THREAD));

    template <Usage u>
    constexpr static std::string_view usageToStr() {
        return array_helpers::find(ThreadUsageToStrMap, u)->second;
    }
    static std::string_view usageToStr(Usage u) {
        return array_helpers::find(ThreadUsageToStrMap, u)->second;
    }

    template <Usage usage, class T = ManagedThread, typename... Args>
        requires std::is_base_of_v<ManagedThread, T> &&
                 std::is_constructible_v<T, Args...>
    T* create(Args&&... args) {
        constexpr std::string_view usageStr = usageToStr<usage>();
        return create_internal<T>(usage, usageStr,
                                  std::forward<Args&&>(args)...);
    }
    template <class T = ManagedThread, typename... Args>
        requires std::is_base_of_v<ManagedThread, T> &&
                 std::is_constructible_v<T, Args...>
    T* create(Usage usage, Args&&... args) {
        std::string_view usageStr = usageToStr(usage);
        return create_internal<T>(usage, usageStr,
                                  std::forward<Args&&>(args)...);
    }

    template <Usage usage, class T = ManagedThread>
        requires std::is_base_of_v<ManagedThread, T>
    T* get() {
        constexpr std::string_view usageStr = usageToStr<usage>();
        return get_internal<T>(usage, usageStr);
    }

    template <class T = ManagedThread>
        requires std::is_base_of_v<ManagedThread, T>
    T* get(Usage usage) {
        std::string_view usageStr = usageToStr(usage);
        return get_internal<T>(usage, usageStr);
    }

    // Stop all controllers managed by this manager, and shutdown this.
    void destroyManager();
    // Destroy a controller given usage
    void destroyController(Usage usage, bool deleteIt = true);

   private:
    template <class T, typename... Args>
    T* create_internal(Usage usage, std::string_view usageStr, Args&&... args);

    template <class T>
    T* get_internal(Usage usage, std::string_view usageStr);

    std::atomic_bool kIsUnderStopAll = false;
    std::shared_mutex mControllerLock;
    std::unordered_map<Usage, std::unique_ptr<ManagedThread>> kControllers;
};

struct TgBotPPImpl_shared_deps_API ManagedThread {
    using thread_function = std::function<void(void)>;
    template <typename T>
        requires std::is_base_of_v<ManagedThread, T>
    using prestop_function_t = std::function<void(T*)>;
    using prestop_function = prestop_function_t<ManagedThread>;

    // Set thread function and run - implictly starts the thread as well
    void runWith(thread_function fn);
    // Set the function called before stopping the thread
    void setPreStopFunction(prestop_function fn);
    // Stop the underlying thread
    void stop();
    // Reset the counter, to make this instance reusable
    void reset();
    // Does this controller have a thread inside it?
    [[nodiscard]] bool isRunning() const;

    template <typename T>
    void onPreStop(prestop_function_t<T> fn) {
        preStop = [fn](ManagedThread* thiz) { fn(static_cast<T*>(thiz)); };
    }
    
    ManagedThread() = default;
    virtual ~ManagedThread() {
        if (isRunning()) {
            stop();
        }
    }

    friend class ThreadManager;

   protected:
    std::atomic_bool kRun = true;
    void delayUnlessStop(const std::chrono::seconds secs) {
        if (!kRun.load()) {
            return;
        }
        std::unique_lock<std::mutex> lk(condvar.mutex);
        condvar.variable.wait_for(lk, secs, [this] { return !kRun.load(); });
    }

   private:
    enum class ControlState {
        UNINITIALIZED,
        STOPPED_PREMATURE,
        STOPPED_BY_STOP_CMD,
        RUNNING,
    } state = ControlState::UNINITIALIZED;

    void _threadFn(thread_function fn);
    void logInvalidState(const char* func);
    std::optional<std::thread> threadP;
    prestop_function preStop;

    struct {
        std::mutex mutex;
        std::condition_variable variable;
    } condvar;
    struct {
        size_t sizeOfThis{};
        struct {
            // It would'nt be a dangling one
            std::string_view str;
            ThreadManager::Usage val;
        } usage;
    } mgr_priv{};
};

struct TgBotPPImpl_shared_deps_API ManagedThreadRunnable : ManagedThread {
    using ManagedThread::ManagedThread;
    using ManagedThread::runWith;
    virtual void runFunction() = 0;
    void run() {
        ManagedThread::runWith([this] { runFunction(); });
    }
    ~ManagedThreadRunnable() override = default;
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
    newIt->mgr_priv.sizeOfThis = sizeof(T);
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
    if (it->second->mgr_priv.sizeOfThis != sizeof(T)) {
        LOG(ERROR) << usageStr << " controller is of wrong type";
        return nullptr;
    }
    return static_cast<T*>(it->second.get());
}
