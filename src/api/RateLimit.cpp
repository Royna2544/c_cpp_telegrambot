#include <absl/log/log.h>
#include <fmt/format.h>

#include <api/RateLimit.hpp>
#include <limits>
#include <utility>

IntervalRateLimiter::IntervalRateLimiter(uint32_t maxPerInterval,
                                         clock::duration interval)
    : maxPerInterval_{maxPerInterval}, interval_{interval} {
    state_.store(0, std::memory_order_relaxed);
}

bool IntervalRateLimiter::check() {
    // 1. Calculate the current "Window ID".
    // We divide the current time by the interval duration.
    auto now = clock::now().time_since_epoch();
    auto current_window = static_cast<uint32_t>(now / interval_);

    // 2. Load the current packed state
    uint64_t expected = state_.load(std::memory_order_acquire);

    // 3. Lock-free Compare-And-Swap (CAS) loop
    while (true) {
        // Unpack the 64-bit state into its two 32-bit halves
        auto state_window = static_cast<uint32_t>(
            expected >> std::numeric_limits<uint32_t>::digits);  // Top 32 bits
        auto state_count = static_cast<uint32_t>(
            expected & std::numeric_limits<uint32_t>::max());  // Bottom 32 bits

        if (state_window != current_window) {
            // We crossed into a new time window.
            // Try to atomically update the window ID and set the count to 1.
            uint64_t desired = (static_cast<uint64_t>(current_window)
                                << std::numeric_limits<uint32_t>::digits) |
                               1;

            if (state_.compare_exchange_weak(expected, desired,
                                             std::memory_order_release,
                                             std::memory_order_acquire)) {
                return true;  // Successfully started a new window!
            }
        } else {
            // We are still in the current time window.
            if (state_count >= maxPerInterval_) {
                return false;  // Rate limit exceeded.
            }

            // Try to atomically increment the count while keeping the same
            // window ID.
            uint64_t desired = (static_cast<uint64_t>(current_window)
                                << std::numeric_limits<uint32_t>::digits) |
                               (state_count + 1);

            if (state_.compare_exchange_weak(expected, desired,
                                             std::memory_order_release,
                                             std::memory_order_acquire)) {
                return true;  // Successfully incremented!
            }
        }

        // If compare_exchange_weak failed, another thread updated state_ right
        // before we did. The 'expected' variable is automatically updated with
        // the new value, and the while loop instantly retries.
    }
}

KeyedIntervalRateLimiter::KeyedIntervalRateLimiter(uint32_t maxPerInterval,
                                                   clock::duration interval)
    : maxPerInterval_{maxPerInterval}, interval_{interval} {}

void KeyedIntervalRateLimiter::pruneIfNeeded(clock::time_point now) {
    if (++checksSincePrune_ < kPruneEvery) {
        return;
    }
    checksSincePrune_ = 0;
    // A key idle for longer than a few windows has an expired limiter window,
    // so dropping it is equivalent to keeping a fresh one for a returning user.
    const auto ttl = interval_ * 4;
    std::erase_if(limiters_, [now, ttl](const auto& kv) {
        return now - kv.second.lastAccess > ttl;
    });
}

bool KeyedIntervalRateLimiter::check(std::int64_t key) {
    const auto now = clock::now();
    const std::lock_guard<std::mutex> lock(mutex_);
    pruneIfNeeded(now);
    auto it = limiters_.find(key);
    if (it == limiters_.end()) {
        it = limiters_
                 .emplace(key, Entry{std::make_unique<IntervalRateLimiter>(
                                         maxPerInterval_, interval_),
                                     now})
                 .first;
    }
    it->second.lastAccess = now;
    return it->second.limiter->check();
}