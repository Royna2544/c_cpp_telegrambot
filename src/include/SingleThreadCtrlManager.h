#include "SingleThreadCtrl.h"

#include <unordered_map>
#include <memory>
#include <type_traits>

class SingleThreadCtrlManager {
 public:
    enum ThreadUsage {
        USAGE_SOCKET_THREAD,
        USAGE_TIMER_THREAD,
        USAGE_SPAMBLOCK_THREAD,
    };
    template <class T = SingleThreadCtrl, std::enable_if_t<std::is_base_of_v<SingleThreadCtrl, T>, bool> = true>
    std::shared_ptr<T> getController(const ThreadUsage usage) {
        std::shared_ptr<SingleThreadCtrl> ptr;
        auto it = kControllers.find(usage);

        if (it != kControllers.end()) {
            LOG_V("Using old: Controller with usage %d", usage);
            ptr = it->second;
        } else {
            LOG_V("New allocation: Controller with usage %d", usage);
            ptr = kControllers[usage] = std::make_shared<T>();
        }
        return std::static_pointer_cast<T>(ptr);
    }
    void destroyController(const ThreadUsage usage) {
        auto it = kControllers.find(usage);

        if (it != kControllers.end()) {
            LOG_V("Deleting: Controller with usage %d", usage);
            it->second.reset();
            kControllers.erase(it);
        } else {
            LOG_W("Not allocated: Controller with usage %d", usage);
        }
    }
    void stopAll() {
        for (const auto &[i, j] : kControllers) {
            LOG_V("Shutdown: Controller with usage %d", i);
            j->stop();
        }
    }
 private:
   std::unordered_map<ThreadUsage, std::shared_ptr<SingleThreadCtrl>> kControllers;
};

extern SingleThreadCtrlManager gSThreadManager;