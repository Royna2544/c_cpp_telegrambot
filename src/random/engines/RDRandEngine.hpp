#pragma once

#ifndef _MSC_VER
#if defined __x86_64__ || defined __i386__
#define RDRAND_MAYBE_SUPPORTED
#include <cpuid.h>
#include <immintrin.h>
#endif // defined __x86_64__ || defined __i386__
#else // _MSC_VER
#if defined(_M_X64) || defined(_M_IX86)
#define RDRAND_MAYBE_SUPPORTED
#include <intrin.h>
#endif // defined(_M_X64) || defined(_M_IX86)
#endif // _MSC_VER

#ifdef RDRAND_MAYBE_SUPPORTED

#ifndef BIT_SET
#define BIT_SET(x, n) ((x) & (1 << n))
#endif

class rdrand_engine {
   public:
    using result_type = uint32_t;

    static constexpr result_type min() { return 0; }
    static constexpr result_type max() { return UINT32_MAX; }

    result_type operator()() const {
        result_type val;
        while (!_rdrand32_step(&val)) {
        }  // retry until success
        return val;
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
        return BIT_SET(ecx, 30);
#else
        int set[4] = {};
        __cpuid(set, 1);
        return BIT_SET(set[2], 30);
#endif
    }
};
#endif
