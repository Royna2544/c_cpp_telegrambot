#pragma once

#ifndef _MSC_VER
#if defined __x86_64__ || defined __i386__
#define RDRAND_MAYBE_SUPPORTED
#include <absl/log/log.h>
#include <cpuid.h>
#include <immintrin.h>
#endif  // defined __x86_64__ || defined __i386__
#else   // _MSC_VER
#if defined(_M_X64) || defined(_M_IX86)
#define RDRAND_MAYBE_SUPPORTED
#include <intrin.h>
#endif  // defined(_M_X64) || defined(_M_IX86)
#endif  // _MSC_VER

#ifdef RDRAND_MAYBE_SUPPORTED

#include <cstdint>
#include <limits>

class rdrand_engine {
   public:
    using result_type = std::uint32_t;

    static constexpr result_type min() { return 0; }
    static constexpr result_type max() {
        return std::numeric_limits<result_type>::max();
    }

    // RDRAND can transiently report failure. Never spin forever: callers can
    // fall back to platform entropy if the bounded retry budget is exhausted.
#if !defined(_MSC_VER)
    __attribute__((target("rdrnd")))
#endif
    bool tryGenerate(result_type& value) const noexcept {
        constexpr unsigned int kMaxAttempts = 10;
        for (unsigned int attempt = 0; attempt < kMaxAttempts; ++attempt) {
            if (_rdrand32_step(&value) != 0) {
                return true;
            }
        }
        return false;
    }

    static bool supported() {
#ifndef _MSC_VER
        unsigned int eax = 0;
        unsigned int ebx = 0;
        unsigned int ecx = 0;
        unsigned int edx = 0;

        if (__get_cpuid(1, &eax, &ebx, &ecx, &edx) == 0) {
            LOG(WARNING) << "CPUID information is not available";
            return false;
        }
        return (ecx & (1U << 30U)) != 0;
#else
        int set[4] = {};
        __cpuid(set, 1);
        return (static_cast<unsigned int>(set[2]) & (1U << 30U)) != 0;
#endif
    }
};
#endif
