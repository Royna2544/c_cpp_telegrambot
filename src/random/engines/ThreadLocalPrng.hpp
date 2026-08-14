#pragma once

#include <array>
#include <cstdint>
#include <random>

namespace random_detail {

using SeedMaterial = std::array<std::uint32_t, 16>;

// Policy::seed() is evaluated exactly once in each participating thread. The
// resulting PRNG is then shared by generate() and shuffle() on that thread.
template <typename Policy>
std::mt19937_64& threadPrng() {
    thread_local std::mt19937_64 engine = [] {
        const auto seedMaterial = Policy::seed();
        std::seed_seq seed(seedMaterial.begin(), seedMaterial.end());
        return std::mt19937_64(seed);
    }();
    return engine;
}

}  // namespace random_detail
