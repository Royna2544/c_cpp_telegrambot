#pragma once

#include <mutex>
#include <shared_mutex>

#include "trivial_helpers/fruit_inject.hpp"

class RefLock {
    std::shared_mutex mutex;

   public:
    std::lock_guard<std::shared_mutex> acquireExclusive() {
        return std::lock_guard<std::shared_mutex>(mutex);
    }

    std::shared_lock<std::shared_mutex> acquireShared() {
        return std::shared_lock<std::shared_mutex>(mutex);
    }

    bool tryAcquireShared() { return mutex.try_lock_shared(); }

    APPLE_INJECT(RefLock()) = default;
};