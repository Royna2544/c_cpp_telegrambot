#ifndef RATELIMITER_H
#define RATELIMITER_H

#include <atomic>
#include <chrono>

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

#endif  // RATELIMITER_H
