#include <gtest/gtest.h>

#include <Random.hpp>
#include <algorithm>
#include <atomic>
#include <string>
#include <thread>
#include <vector>

TEST(RandomTest, ShuffleTerminatesAndPreservesElements) {
    Random random;
    std::vector<std::string> values;
    for (int i = 0; i < 128; ++i) {
        values.emplace_back(std::to_string(i));
    }
    const auto original = values;

    random.shuffle(values);

    auto sortedOriginal = original;
    auto sortedValues = values;
    std::ranges::sort(sortedOriginal);
    std::ranges::sort(sortedValues);
    EXPECT_EQ(sortedValues, sortedOriginal);
}

TEST(RandomTest, ConcurrentDrawsStayWithinRange) {
    Random random;
    constexpr int kThreadCount = 8;
    constexpr int kDrawsPerThread = 2000;
    std::atomic_bool valid = true;
    std::vector<std::thread> threads;
    threads.reserve(kThreadCount);

    for (int i = 0; i < kThreadCount; ++i) {
        threads.emplace_back([&] {
            for (int draw = 0; draw < kDrawsPerThread; ++draw) {
                const auto value = random.generate(7, 31);
                if (value < 7 || value > 31) {
                    valid.store(false, std::memory_order_relaxed);
                }
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_TRUE(valid.load(std::memory_order_relaxed));
}
