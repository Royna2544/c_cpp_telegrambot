#pragma once

#include <mutex>
#include <shared_mutex>

#include "trivial_helpers/fruit_inject.hpp"

class RefLock {
    std::shared_mutex mutex;

   public:
    using ExclusiveLease = std::unique_lock<std::shared_mutex>;
    using SharedLease = std::shared_lock<std::shared_mutex>;

    ExclusiveLease acquireExclusive() {
        return ExclusiveLease(mutex);
    }

    SharedLease acquireShared() {
        return SharedLease(mutex);
    }

    bool tryAcquireShared() { return mutex.try_lock_shared(); }

    APPLE_INJECT(RefLock()) = default;
};