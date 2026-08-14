#include "Random.hpp"

#include <absl/log/log.h>

#include <array>
#include <cstdint>
#include <memory>
#include <random>
#include <string_view>

namespace {

using Prng = std::mt19937_64;

Prng makePrng() {
    // std::random_device is backed by the operating system on the supported
    // targets. Consume several words once per thread, then use a PRNG for all
    // runtime draws so commands never drain or block on a hardware RNG device.
    std::array<std::uint32_t, 8> seedData{};
    {
        std::random_device rd;
        std::ranges::generate(seedData, [&rd] { return rd(); });
    }
    std::seed_seq seed(seedData.begin(), seedData.end());
    return Prng(seed);
}

Prng& threadPrng() {
    thread_local Prng engine = makePrng();
    return engine;
}

struct ThreadLocalPrng : Random::ImplBase {
    bool isSupported() const override { return true; }

    Random::ret_type generate(const Random::ret_type min,
                              const Random::ret_type max) const override {
        auto& engine = threadPrng();
        return gen_impl(&engine, min, max);
    }

    void shuffle(std::vector<std::string>& it) const override {
        auto& engine = threadPrng();
        ShuffleImpl(it, &engine);
    }

    [[nodiscard]] std::string_view getName() const override {
        return "OS-seeded thread-local PRNG";
    }
    ~ThreadLocalPrng() override = default;
};

}  // namespace

Random::Random() {
    impl_ = std::make_unique<ThreadLocalPrng>();
    LOG(INFO) << "Using " << impl_->getName() << " as RNG impl";
}

void Random::shuffle(std::vector<std::string>& array) const {
    impl_->shuffle(array);
}

Random::ret_type Random::generate(const ret_type min,
                                  const ret_type max) const {
    return impl_->generate(min, max);
}
