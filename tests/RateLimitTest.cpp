#include <gtest/gtest.h>

#include <api/RateLimit.hpp>
#include <chrono>
#include <cstdint>

using std::chrono_literals::operator""h;

// A 1-hour interval keeps every check within a single window, so these tests
// exercise the counting / per-key logic deterministically (no wall-clock race).

TEST(IntervalRateLimiter, AllowsUpToMaxThenBlocks) {
    IntervalRateLimiter limiter(2, 1h);
    EXPECT_TRUE(limiter.check());
    EXPECT_TRUE(limiter.check());
    EXPECT_FALSE(limiter.check());  // third within the window is blocked
    EXPECT_FALSE(limiter.check());
}

TEST(KeyedIntervalRateLimiter, PerKeyBudgetsAreIndependent) {
    KeyedIntervalRateLimiter limiter(2, 1h);

    EXPECT_TRUE(limiter.check(1));
    EXPECT_TRUE(limiter.check(1));
    EXPECT_FALSE(limiter.check(1));  // key 1 exhausted

    // key 2 has its own budget, unaffected by key 1
    EXPECT_TRUE(limiter.check(2));
    EXPECT_TRUE(limiter.check(2));
    EXPECT_FALSE(limiter.check(2));
}

TEST(KeyedIntervalRateLimiter, OneHeavyKeyDoesNotStarveOthers) {
    KeyedIntervalRateLimiter limiter(1, 1h);

    // One key burns its whole budget...
    EXPECT_TRUE(limiter.check(100));
    EXPECT_FALSE(limiter.check(100));

    // ...everyone else is still served (the bug this guards against: a single
    // global limiter let one user block all others).
    for (std::int64_t key = 200; key < 210; ++key) {
        EXPECT_TRUE(limiter.check(key)) << "key " << key << " should be allowed";
    }
}
