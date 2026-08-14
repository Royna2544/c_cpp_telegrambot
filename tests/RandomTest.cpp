#include <gtest/gtest.h>

#include <Random.hpp>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <engines/ThreadLocalPrng.hpp>
#include <string>
#include <thread>
#include <vector>

namespace {

struct CountingSeedPolicy {
    static inline std::atomic_uint seedCalls = 0;

    static random_detail::SeedMaterial seed() noexcept {
        seedCalls.fetch_add(1, std::memory_order_relaxed);
        random_detail::SeedMaterial material{};
        std::uint32_t value = 1;
        for (auto& word : material) {
            word = value++;
        }
        return material;
    }
};

}  // namespace

TEST(RandomTest, ExplicitStdCppSelectionIsStable) {
    Random random(Random::Backend::StdCpp);

    EXPECT_TRUE(Random::isBackendSupported(Random::Backend::StdCpp));
    EXPECT_EQ(random.selectedBackend(), Random::Backend::StdCpp);
}

TEST(RandomTest, AutoSelectsFirstSupportedBackend) {
    const auto expected =
        Random::isBackendSupported(Random::Backend::RDRand)
            ? Random::Backend::RDRand
            : (Random::isBackendSupported(Random::Backend::KernelRand)
                   ? Random::Backend::KernelRand
                   : Random::Backend::StdCpp);

    Random random;

    EXPECT_EQ(random.selectedBackend(), expected);
}

TEST(RandomTest, UnsupportedExplicitBackendFallsBackToStdCpp) {
    constexpr Random::Backend candidates[] = {
        Random::Backend::RDRand,
        Random::Backend::KernelRand,
    };

    for (const auto backend : candidates) {
        if (!Random::isBackendSupported(backend)) {
            Random random(backend);
            EXPECT_EQ(random.selectedBackend(), Random::Backend::StdCpp);
            return;
        }
    }

    GTEST_SKIP() << "Every platform backend is available on this host";
}

TEST(RandomTest, ShuffleTerminatesAndPreservesElements) {
    // Explicitly exercise StdCpp, one of the formerly recursive overloads.
    Random random(Random::Backend::StdCpp);
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

TEST(RandomTest, ThreadLocalPrngReadsItsEntropySourceOnlyOnce) {
    CountingSeedPolicy::seedCalls.store(0, std::memory_order_relaxed);

    std::thread worker([] {
        for (int draw = 0; draw < 1000; ++draw) {
            (void)random_detail::threadPrng<CountingSeedPolicy>()();
        }
        std::vector<std::string> values = {"a", "b", "c", "d"};
        for (int shuffle = 0; shuffle < 100; ++shuffle) {
            auto& engine = random_detail::threadPrng<CountingSeedPolicy>();
            std::shuffle(values.begin(), values.end(), engine);
        }
    });
    worker.join();

    EXPECT_EQ(CountingSeedPolicy::seedCalls.load(std::memory_order_relaxed),
              1U);
}
