#ifndef RATELIMITER_H
#define RATELIMITER_H

#include <atomic>
#include <chrono>

/**
 * A rate limiter that can rate limit events to N events per M milliseconds.
 *
 * It is intended to be fast to check when messages are not being rate limited.
 * When messages are being rate limited it is slightly slower, as it has to
 * check the clock each time check() is called in this case.
 */
class IntervalRateLimiter {
   public:
    using clock = std::chrono::steady_clock;

    constexpr IntervalRateLimiter(uint64_t maxPerInterval,
                                  clock::duration interval)
        : maxPerInterval_{maxPerInterval}, interval_{interval} {}

    bool check();

   private:
    // First check should always succeed, so initial timestamp is at the
    // beginning of time.
    static_assert(std::is_signed<clock::rep>::value,
                  "Need signed time point to represent initial time");
    constexpr static auto kInitialTimestamp =
        std::numeric_limits<clock::rep>::min();

    bool checkSlow();

    const uint64_t maxPerInterval_;
    const clock::time_point::duration interval_;

    // Initialize count_ to the maximum possible value so that the first
    // call to check() will call checkSlow() to initialize timestamp_,
    // but subsequent calls will hit the fast-path and avoid checkSlow()
    std::atomic<uint64_t> count_{std::numeric_limits<uint64_t>::max()};
    // Ideally timestamp_ would be a
    // std::atomic<clock::time_point>, but this does not
    // work since time_point's constructor is not noexcept
    std::atomic<clock::rep> timestamp_{kInitialTimestamp};
};

#endif  // RATELIMITER_H
