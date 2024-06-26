#include "RandomNumberGenerator.h"

#include <absl/log/log.h>

#include <BackendChooser.hpp>
#include <algorithm>
#include <cassert>
#include <iomanip>
#include <memory>
#include <random>
#include <string_view>

#include "InstanceClassBase.hpp"
#include "KernelRandEngine.h"
#include "RDRandEngine.h"

using return_type = random_return_type;

/**
 * @brief      Base class for random number generators.
 *
 * This class provides an interface for random number generators. It defines
 * functions for generating random numbers and shuffling containers. The
 * isSupported() function can be used to determine if a particular RNG is
 * available on the system.
 *
 * @tparam     random_return_type  The type of random numbers generated by the
 *                                 RNG.
 */
struct RNGBase {
    /**
     * @brief      Determines if the RNG is isSupported on the system.
     *
     * @return     `true` if the RNG is isSupported, `false` otherwise.
     */
    virtual bool isSupported() const = 0;

    /**
     * @brief      Generates a random number in the specified range.
     *
     * @param[in]  min  The minimum value of the random number.
     * @param[in]  max  The maximum value of the random number.
     *
     * @return     A random number in the specified range.
     */
    virtual random_return_type generate(const random_return_type min,
                                        const random_return_type max) const = 0;

    /**
     * @brief      Shuffles the elements in a container.
     *
     * This function uses the random number generator to rearrange the elements
     * in a container. The specific algorithm used for shuffling is dependent on
     * the RNG implementation.
     *
     * @deprecated This function is a template, can't be overriden therefore a
     * stub
     * @tparam     T  The type of elements in the container.
     * @param[in,out]  it  The container to be shuffled.
     */
    template <typename T>
    void shuffle(std::vector<T>& it) const;

    /**
     * @brief      Shuffles the elements in a string container.
     *
     * This function uses the random number generator to rearrange the elements
     * in a container. The specific algorithm used for shuffling is dependent on
     * the RNG implementation.
     */
    virtual void shuffle_string(std::vector<std::string>& it) const = 0;

    /**
     * @brief      Returns the name of the RNG.
     *
     * @return     A string containing the name of the RNG.
     */
    virtual const std::string_view getName() const = 0;

    /**
     * @brief      Shuffles the elements in a container using the specified
     * random number engine.
     *
     * This function uses the specified random number engine to rearrange the
     * elements in a container. The specific algorithm used for shuffling is
     * dependent on the RNG implementation.
     *
     * @tparam     Engine  The type of random number engine to be used.
     * @tparam     T  The type of elements in the container.
     * @param[in,out]  in  The container to be shuffled.
     * @param[in]  e  The random number engine to be used for shuffling.
     */
    template <class Engine, typename T>
    void ShuffleImpl(std::vector<T>& in, Engine* e) const {
        std::shuffle(in.begin(), in.end(), *e);
    }

    /**
     * @brief      Generates a random number in the specified range using the
     *             specified random number generator.
     *
     * This function uses the specified random number generator to generate a
     * random number in the specified range.
     *
     * @tparam     Generator  The type of random number generator to be used.
     * @param[in]  gen  The random number generator to be used for generating
     * the random number.
     * @param[in]  min  The minimum value of the random number.
     * @param[in]  max  The maximum value of the random number.
     *
     * @return     A random number in the specified range.
     */
    template <class Generator>
    random_return_type genRandomNumberImpl(Generator* gen,
                                           random_return_type min,
                                           random_return_type max) const {
        if (min > max) {
            LOG(WARNING) << "min(" << min << ") is bigger than max(" << max
                         << ")";
            std::swap(min, max);
        } else if (min == max) {
            LOG(WARNING) << "min == max == " << min;
            return min;
        }
        std::uniform_int_distribution<return_type> distribution(min, max);
        return distribution(*gen);
    }
    virtual ~RNGBase() = default;
};

struct StdCpp : RNGBase {
    static std::mt19937 RNG_std_create_rng(void) {
        std::random_device rd;
        std::mt19937 gen(rd());
        return gen;
    }

    return_type generate(const return_type min,
                         const return_type max) const override {
        auto e = RNG_std_create_rng();
        return genRandomNumberImpl(&e, min, max);
    }

    bool isSupported(void) const override { return true; }

    template <typename T>
    void shuffle(std::vector<T>& in) const {
        auto e = RNG_std_create_rng();
        ShuffleImpl(in, &e);
    }

    void shuffle_string(std::vector<std::string>& it) const override {
        shuffle(it);
    }

    const std::string_view getName(void) const override {
        return "STD C++ pesudo RNG";
    }
    ~StdCpp() override = default;
};

#ifdef RDRAND_MAYBE_SUPPORTED
struct RDRand : RNGBase {
    return_type generate(const return_type min,
                         const return_type max) const override {
        return genRandomNumberImpl(&engine, min, max);
    }

    bool isSupported(void) const override {
        unsigned int eax = 0;
        unsigned int ebx = 0;
        unsigned int ecx = 0;
        unsigned int edx = 0;

        if (__get_cpuid(1, &eax, &ebx, &ecx, &edx) == 0) {
            LOG(WARNING) << "CPUID information is not available";
            return false;
        }
        return BIT_SET(ecx, 30);
    }

    template <typename T>
    void shuffle(std::vector<T>& in) const {
        ShuffleImpl(in, &engine);
    }

    void shuffle_string(std::vector<std::string>& it) const override {
        shuffle(it);
    }

    const std::string_view getName() const override {
        return "X86 RDRAND instr. HWRNG (Intel/AMD)";
    }
    ~RDRand() override = default;

   private:
    rdrand_engine engine;
};
#endif

#ifdef KERNELRAND_MAYBE_SUPPORTED
struct KernelRand : RNGBase {
    return_type generate(const return_type min,
                         const return_type max) const override {
        return genRandomNumberImpl(kernel_rand_engine::getInstance().get(), min,
                                   max);
    }

    bool isSupported(void) const override {
        return kernel_rand_engine::getInstance()->isSupported();
    }

    template <typename T>
    void shuffle(std::vector<T>& in) const {
        ShuffleImpl(in, kernel_rand_engine::getInstance().get());
    }

    void shuffle_string(std::vector<std::string>& it) const override {
        shuffle(it);
    }

    const std::string_view getName() const override {
        return "Linux/MacOS HWRNG interface";
    }
    ~KernelRand() override = default;
};
#endif

#ifdef RDRAND_MAYBE_SUPPORTED
#define RDRAND_CLASS RDRand,
#else
#define RDRAND_CLASS
#endif

#ifdef KERNELRAND_MAYBE_SUPPORTED
#define KERNELRAND_CLASS KernelRand,
#else
#define KERNELRAND_CLASS
#endif

#define IMPL_LIST KERNELRAND_CLASS RDRAND_CLASS StdCpp

struct RandomBackendChooser : BackendChooser<RNGBase, IMPL_LIST> {
    ~RandomBackendChooser() override = default;

    void onMatchFound(RNGBase& obj) override {
        LOG(INFO) << "Using " << std::quoted(obj.getName())
                  << " for random number generation";
    }
};

static RNGBase* getRNG(void) {
    static RandomBackendChooser chooser;
    static RNGBase* rng = nullptr;
    if (rng == nullptr) {
        rng = chooser.getObject();
    }
    return rng;
}

namespace RandomNumberGenerator {

return_type generate(const return_type min, const return_type max) {
    return getRNG()->generate(min, max);
}

return_type generate(const return_type max) {
    return generate(0, max);
}

void shuffleStringArray(std::vector<std::string>& inArray) {
    getRNG()->shuffle_string(inArray);
}

}  // namespace RandomNumberGenerator

#ifdef KERNELRAND_MAYBE_SUPPORTED
DECLARE_CLASS_INST(kernel_rand_engine);
#endif