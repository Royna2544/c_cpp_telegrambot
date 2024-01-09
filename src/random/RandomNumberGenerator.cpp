#include <../utils/LinuxPort.h>
#include <Logging.h>
#include <RuntimeException.h>

#include <algorithm>
#include <cassert>
#include <functional>
#include <random>

#include "RandomNumberGenerator.h"
#include "KernelRandEngine.h"
#include "RDRandEngine.h"

template <typename T>
using shuffle_handle_t = std::function<void(std::vector<T> &)>;
using return_type = random_return_type;

template <class Generator>
return_type genRandomNumberImpl(Generator gen, const return_type min, const return_type max) {
    if (min >= max) {
        throw runtime_errorf("%s: Assert failed: min(%d) is bigger than max(%d)", __func__, min, max);
    }
    std::uniform_int_distribution<return_type> distribution(min, max);
    return distribution(gen);
}

struct RNGType {
    std::function<bool(void)> supported;
    std::function<return_type(const return_type min, const return_type max)> generate;
    shuffle_handle_t<std::string> shuffle_string;
    const char* name;
};

static std::mt19937 RNG_std_create_rng(void) {
    std::random_device rd;
    std::mt19937 gen(rd());
    WARN_ONCE(rd.entropy() == 0, "Pesudo RNG detected");
    return gen;
}

static return_type RNG_std_generate(const return_type min, const return_type max) {
    return genRandomNumberImpl(RNG_std_create_rng(), min, max);
}

static bool RNG_std_supported(void) {
    return true;
}

template <typename T>
void RNG_std_shuffle(std::vector<T>& in) {
    std::shuffle(in.begin(), in.end(), RNG_std_create_rng());
}

#ifdef RDRAND_MAYBE_SUPPORTED
static int RNG_rdrand_generate(const return_type min, const return_type max) {
    rdrand_engine gen;
    return genRandomNumberImpl(gen, min, max);
}

static bool RNG_rdrand_supported(void) {
    unsigned int eax, ebx, ecx, edx;

    if (__get_cpuid(1, &eax, &ebx, &ecx, &edx) == 0) {
        LOG_W("CPUID information is not available");
        return false;
    }
    return BIT_SET(ecx, 30);
}

template <typename T>
void RNG_rdrand_shuffle(std::vector<T>& in) {
    std::shuffle(in.begin(), in.end(), rdrand_engine());
}
#endif

#ifdef KERNELRAND_MAYBE_SUPPORTED
static return_type RNG_kernrand_generate(const return_type min, const return_type max) {
    static kernel_rand_engine gen;
    return genRandomNumberImpl(gen, min, max);
}

static bool RNG_kernrand_supported(void) {
    return kernel_rand_engine::isSupported();
}

template <typename T>
static void RNG_kernrand_shuffle(std::vector<T>& in) {
    std::shuffle(in.begin(), in.end(), kernel_rand_engine());
}
#endif

struct RNGType RNGs[] = {
#ifdef RDRAND_MAYBE_SUPPORTED
    {
        .supported = RNG_rdrand_supported,
        .generate = RNG_rdrand_generate,
        .shuffle_string = RNG_rdrand_shuffle<std::string>,
        .name = "X86 RDRAND instr. HWRNG (Intel/AMD)",
    },
#endif
#ifdef KERNELRAND_MAYBE_SUPPORTED
    {
        .supported = RNG_kernrand_supported,
        .generate = RNG_kernrand_generate,
        .shuffle_string = RNG_kernrand_shuffle<std::string>,
        .name = "Linux/MacOS HWRNG interface",
    },
#endif
    {
        .supported = RNG_std_supported,
        .generate = RNG_std_generate,
        .shuffle_string = RNG_std_shuffle<std::string>,
        .name = "STD C++ pesudo RNG",
    },
};

static RNGType* getRNG(void) {
    static RNGType* rng = nullptr;

    if (!rng) {
        for (size_t i = 0; i < sizeof(RNGs) / sizeof(RNGType); ++i) {
            auto thisrng = &RNGs[i];
            if (thisrng->supported()) {
                LOG_I("Using '%s' for random number generation", thisrng->name);
                rng = thisrng;
                break;
            }
        }
        // We don't really need fallback, as libc++'s rng is always supported
	// But this looks better
        assert(rng != nullptr);
    }
    return rng;
}

return_type genRandomNumber(const return_type min, const return_type max) {
    return getRNG()->generate(min, max);
}

return_type genRandomNumber(const return_type max) {
    return genRandomNumber(0, max);
}

void shuffleStringArray(std::vector<std::string>& in) {
    getRNG()->shuffle_string(in);
}
