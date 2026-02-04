#include <absl/log/log.h>
#include <fmt/format.h>

#include <api/RateLimit.hpp>

bool IntervalRateLimiter::check() {
    DLOG_EVERY_N_SEC(INFO, 5) << fmt::format("count: {}, timestamp: {}",
                                             count_.load(), timestamp_.load());
    auto origCount = count_.fetch_add(1, std::memory_order_acq_rel);
    if (origCount < maxPerInterval_) {
        // We did not hit the rate limit cap.
        DLOG_EVERY_N_SEC(INFO, 5) << "FastPath: Allowed";
        return true;
    }
    return checkSlow();
}

bool IntervalRateLimiter::checkSlow() {
    auto ts = timestamp_.load();
    auto now = clock::now().time_since_epoch().count();
    if (now < (ts + interval_.count())) {
        DLOG_EVERY_N_SEC(INFO, 5)
            << "SlowPath: RateLimited - interval not passed";
        // We fell into the previous interval.
        return false;
    }

    if (!timestamp_.compare_exchange_strong(ts, now)) {
        // We raced with another thread that reset the timestamp.
        // We treat this as if we fell into the previous interval, and so we
        // rate-limit ourself.
        DLOG_EVERY_N_SEC(INFO, 5)
            << "SlowPath: RateLimited - reset timestamp race";
        return false;
    }

    if (ts == kInitialTimestamp) {
        // If we initialized timestamp_ for the very first time increment count_
        // by one instead of setting it to 0.  Our original increment made it
        // roll over to 0, so other threads may have already incremented it
        // again and passed the check.
        auto origCount = count_.fetch_add(1, std::memory_order_acq_rel);
        // Check to see if other threads already hit the rate limit cap before
        // we finished checkSlow().
        return (origCount < maxPerInterval_);
    }

    // In the future, if we wanted to return the number of dropped events we
    // could use (count_.exchange(0) - maxPerInterval_) here.
    count_.store(1, std::memory_order_release);
    DLOG_EVERY_N_SEC(INFO, 5) << "SlowPath: Allowed";
    // We passed the check.
    return true;
}