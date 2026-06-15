#ifndef RATELIMITER_H
#define RATELIMITER_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>

class IntervalRateLimiter {
   public:
    using clock = std::chrono::steady_clock;

    IntervalRateLimiter(uint32_t maxPerInterval, clock::duration interval);
    bool check();

   private:
    const uint32_t maxPerInterval_;
    const clock::duration interval_;

    // PACKED STATE:
    // [ Top 32 bits: Window ID ] [ Bottom 32 bits: Request Count ]
    std::atomic<uint64_t> state_{0};
};

// Maintains an independent IntervalRateLimiter per key (e.g. per user id), so
// one heavy user cannot consume a single shared global budget and starve
// everyone else. Idle keys are pruned periodically to bound memory.
class KeyedIntervalRateLimiter {
   public:
    using clock = IntervalRateLimiter::clock;

    KeyedIntervalRateLimiter(uint32_t maxPerInterval, clock::duration interval);
    bool check(std::int64_t key);

   private:
    struct Entry {
        std::unique_ptr<IntervalRateLimiter> limiter;
        clock::time_point lastAccess;
    };

    void pruneIfNeeded(clock::time_point now);  // called under mutex_

    const uint32_t maxPerInterval_;
    const clock::duration interval_;
    std::mutex mutex_;
    std::unordered_map<std::int64_t, Entry> limiters_;
    std::size_t checksSincePrune_{0};

    static constexpr std::size_t kPruneEvery = 256;
};

#endif  // RATELIMITER_H
