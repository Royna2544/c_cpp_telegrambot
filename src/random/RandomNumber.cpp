#include <../utils/LinuxPort.h>
#include <Logging.h>
#include <RuntimeException.h>

#include <cassert>
#include <random>

#include "KernelRandEngine.h"
#include "RDRandEngine.h"

template <class Generator>
int genRandomNumberImpl(Generator& gen, const int min, const int max) {
    if (min >= max) {
        throw runtime_errorf("min(%d) >= max(%d)", min, max);
    }
    std::uniform_int_distribution<int> distribution(min, max);
    return distribution(gen);
}

struct RNGType {
    bool (*supported)(void);
    int (*generate)(const int min, const int max);
    const char* name;
};

static int RNG_std_generate(const int min, const int max) {
    std::random_device rd;
    std::mt19937 gen(rd());
    WARN_ONCE(rd.entropy() == 0, "Pesudo RNG detected");
    return genRandomNumberImpl(gen, min, max);
}

static bool RNG_std_supported(void) {
    return true;
}

#ifdef RDRAND_MAYBE_SUPPORTED
static int RNG_rdrand_generate(const int min, const int max) {
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
#endif

#ifdef KERNELRAND_MAYBE_SUPPORTED
static int RNG_kernrand_generate(const int min, const int max) {
    kernel_rand_engine gen;
    return genRandomNumberImpl(gen, min, max);
}

static bool RNG_kernrand_supported(void) {
    return kernel_rand_engine::isSupported();
}
#endif

struct RNGType RNGs[] = {
#ifdef RDRAND_MAYBE_SUPPORTED
    {
        .supported = RNG_rdrand_supported,
        .generate = RNG_rdrand_generate,
        .name = "X86 RDRAND instr. HWRNG (Intel/AMD)",
    },
#endif
#ifdef KERNELRAND_MAYBE_SUPPORTED
    {
        .supported = RNG_kernrand_supported,
        .generate = RNG_kernrand_generate,
        .name = "Linux/MacOS hwrng interface",
    },
#endif
    {
        .supported = RNG_std_supported,
        .generate = RNG_std_generate,
        .name = "libstdc++ pesudo RNG",
    },
};

int genRandomNumber(const int min, const int max) {
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
        assert(rng != nullptr);
    }
    return rng->generate(min, max);
}

int genRandomNumber(const int max) {
    return genRandomNumber(0, max);
}
