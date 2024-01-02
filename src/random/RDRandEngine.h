#pragma once

#if defined __x86_64__ || defined __i386__

#define RDRAND_MAYBE_SUPPORTED

#include <cpuid.h>
#include <immintrin.h>

#include <random>

#ifndef BIT_SET
#define BIT_SET(x, n) ((x) & (1 << n))
#endif

class rdrand_engine {
   public:
    using result_type = uint32_t;

    static constexpr result_type min() { return 0; }
    static constexpr result_type max() { return UINT32_MAX; }

    result_type operator()() {
        result_type val;
        while (!_rdrand32_step(&val)) {
        }  // retry until success
        return val;
    }
};
#endif
