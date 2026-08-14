#include "Random.hpp"

#include <absl/log/log.h>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
// windows.h defines the NT types consumed by bcrypt.h; keep this order.
// clang-format off
#include <windows.h>
#include <bcrypt.h>
// clang-format on
#endif

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <random>
#include <string_view>
#include <thread>
#include <utility>

#include "engines/KernelRandEngine.hpp"
#include "engines/RDRandEngine.hpp"
#include "engines/ThreadLocalPrng.hpp"

namespace {

using Backend = Random::Backend;
using MixIn = std::array<std::uint32_t, 8>;
using random_detail::SeedMaterial;

std::atomic_uint64_t gFallbackSequence = 0;

std::uint64_t splitMix64(std::uint64_t value) noexcept {
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

void fillEmergencySeed(std::uint32_t* output,
                       const std::size_t count) noexcept {
    const auto steady = static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const auto wall = static_cast<std::uint64_t>(
        std::chrono::system_clock::now().time_since_epoch().count());
    const auto thread = static_cast<std::uint64_t>(
        std::hash<std::thread::id>{}(std::this_thread::get_id()));
    const auto sequence =
        gFallbackSequence.fetch_add(1, std::memory_order_relaxed);
    auto state =
        steady ^ (wall << 1U) ^ (thread << 2U) ^ sequence ^
        static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(output));
    for (std::size_t index = 0; index < count; ++index) {
        state = splitMix64(state);
        output[index] = static_cast<std::uint32_t>(state ^ (state >> 32U));
    }
}

bool tryFillStdCpp(std::uint32_t* output, const std::size_t count) noexcept {
    try {
        std::random_device source;
        for (std::size_t index = 0; index < count; ++index) {
            output[index] = source();
        }
        return true;
    } catch (const std::exception& error) {
        LOG(WARNING) << "std::random_device failed: " << error.what();
    } catch (...) {
        LOG(WARNING) << "std::random_device failed";
    }
    return false;
}

bool tryFillNative(std::uint32_t* output, const std::size_t count) noexcept {
    if (output == nullptr || count == 0) {
        return count == 0;
    }
#ifdef _WIN32
    if (count > static_cast<std::size_t>(std::numeric_limits<ULONG>::max()) /
                    sizeof(std::uint32_t)) {
        return false;
    }
    const auto size = count * sizeof(std::uint32_t);
    const auto status = BCryptGenRandom(
        nullptr, reinterpret_cast<PUCHAR>(output), static_cast<ULONG>(size),
        BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    return BCRYPT_SUCCESS(status);
#elif defined KERNELRAND_MAYBE_SUPPORTED
    return kernel_rand_engine::fill(output, count);
#else
    (void)output;
    (void)count;
    return false;
#endif
}

SeedMaterial nativeSeedMaterial() noexcept {
    SeedMaterial seedMaterial{};
    if (tryFillNative(seedMaterial.data(), seedMaterial.size())) {
        return seedMaterial;
    }

    LOG(WARNING) << "Native OS entropy failed; using std::random_device";
    if (!tryFillStdCpp(seedMaterial.data(), seedMaterial.size())) {
        LOG(WARNING) << "Using emergency non-cryptographic PRNG seed";
        fillEmergencySeed(seedMaterial.data(), seedMaterial.size());
    }
    return seedMaterial;
}

void xorMixIn(SeedMaterial& seedMaterial, const MixIn& mixIn) noexcept {
    constexpr auto kOffset = SeedMaterial{}.size() - MixIn{}.size();
    for (std::size_t index = 0; index < mixIn.size(); ++index) {
        seedMaterial[kOffset + index] ^= mixIn[index];
    }
}

struct StdCppPolicy {
    static SeedMaterial seed() noexcept {
        auto seedMaterial = nativeSeedMaterial();
        MixIn mixIn{};
        if (!tryFillStdCpp(mixIn.data(), mixIn.size())) {
            fillEmergencySeed(mixIn.data(), mixIn.size());
        }
        xorMixIn(seedMaterial, mixIn);
        return seedMaterial;
    }
};

#ifdef RDRAND_MAYBE_SUPPORTED
struct RDRandPolicy {
    static SeedMaterial seed() noexcept {
        auto seedMaterial = nativeSeedMaterial();
        MixIn mixIn{};
        rdrand_engine source;
        bool complete = true;
        for (auto& word : mixIn) {
            if (!source.tryGenerate(word)) {
                complete = false;
                break;
            }
        }
        if (!complete) {
            LOG(WARNING) << "RDRAND transiently failed; using "
                            "std::random_device mix-in";
            if (!tryFillStdCpp(mixIn.data(), mixIn.size())) {
                fillEmergencySeed(mixIn.data(), mixIn.size());
            }
        }
        xorMixIn(seedMaterial, mixIn);
        return seedMaterial;
    }
};
#endif

struct KernelRandPolicy {
    static SeedMaterial seed() noexcept { return nativeSeedMaterial(); }
};

template <typename Policy>
struct ThreadLocalBackend : Random::ImplBase {
    Random::ret_type generate(const Random::ret_type min,
                              const Random::ret_type max) const override {
        auto& engine = random_detail::threadPrng<Policy>();
        return gen_impl(&engine, min, max);
    }

    void shuffle(std::vector<std::string>& values) const override {
        auto& engine = random_detail::threadPrng<Policy>();
        ShuffleImpl(values, &engine);
    }
};

struct StdCpp final : ThreadLocalBackend<StdCppPolicy> {
    bool isSupported() const override { return true; }

    [[nodiscard]] std::string_view getName() const override {
        return "STD C++ entropy-seeded PRNG";
    }
};

#ifdef RDRAND_MAYBE_SUPPORTED
struct RDRand final : ThreadLocalBackend<RDRandPolicy> {
    bool isSupported() const override { return rdrand_engine::supported(); }

    [[nodiscard]] std::string_view getName() const override {
        return "X86 RDRAND-seeded PRNG (Intel/AMD)";
    }
};
#endif

#ifdef KERNELRAND_MAYBE_SUPPORTED
struct KernelRand final : ThreadLocalBackend<KernelRandPolicy> {
    bool isSupported() const override {
        return kernel_rand_engine::supported();
    }

    [[nodiscard]] std::string_view getName() const override {
        return "Linux/macOS kernel entropy-seeded PRNG";
    }
};
#endif

std::unique_ptr<Random::ImplBase> makeBackend(const Backend backend) {
    switch (backend) {
#ifdef RDRAND_MAYBE_SUPPORTED
        case Backend::RDRand:
            return std::make_unique<RDRand>();
#endif
#ifdef KERNELRAND_MAYBE_SUPPORTED
        case Backend::KernelRand:
            return std::make_unique<KernelRand>();
#endif
        case Backend::Auto:
            return nullptr;
        case Backend::StdCpp:
            return std::make_unique<StdCpp>();
        default:
            return nullptr;
    }
}

}  // namespace

Random::Random() : Random(Backend::Auto) {}

Random::Random(const Backend requestedBackend) {
    const auto select = [this](const Backend backend) {
        auto candidate = makeBackend(backend);
        if (candidate != nullptr && candidate->isSupported()) {
            selectedBackend_ = backend;
            impl_ = std::move(candidate);
            return true;
        }
        return false;
    };

    if (requestedBackend == Backend::Auto) {
        if (!select(Backend::RDRand) && !select(Backend::KernelRand)) {
            (void)select(Backend::StdCpp);
        }
    } else if (!select(requestedBackend)) {
        LOG(WARNING) << "Requested RNG backend is unavailable; using StdCpp";
        (void)select(Backend::StdCpp);
    }

    LOG(INFO) << "Using " << impl_->getName() << " as RNG impl";
}

bool Random::isBackendSupported(const Backend backend) noexcept {
    switch (backend) {
        case Backend::Auto:
        case Backend::StdCpp:
            return true;
        case Backend::RDRand:
#ifdef RDRAND_MAYBE_SUPPORTED
            return rdrand_engine::supported();
#else
            return false;
#endif
        case Backend::KernelRand:
#ifdef KERNELRAND_MAYBE_SUPPORTED
            return kernel_rand_engine::supported();
#else
            return false;
#endif
    }
    return false;
}

void Random::shuffle(std::vector<std::string>& array) const {
    impl_->shuffle(array);
}

Random::ret_type Random::generate(const ret_type min,
                                  const ret_type max) const {
    return impl_->generate(min, max);
}
