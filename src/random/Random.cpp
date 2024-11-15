#include "Random.hpp"

#include <absl/log/log.h>

#include <cassert>
#include <memory>
#include <random>
#include <string_view>

#include "engines/KernelRandEngine.hpp"
#include "engines/RDRandEngine.hpp"

struct StdCpp : Random::ImplBase {
    static std::mt19937 RNG_std_create_rng() {
        std::random_device rd;
        std::mt19937 gen(rd());
        return gen;
    }

    Random::ret_type generate(const Random::ret_type min,
                              const Random::ret_type max) const override {
        auto e = RNG_std_create_rng();
        return gen_impl(&e, min, max);
    }

    bool isSupported(void) const override { return true; }

    template <typename T>
    void shuffle(std::vector<T>& in) const {
        auto e = RNG_std_create_rng();
        ShuffleImpl(in, &e);
    }

    void shuffle(std::vector<std::string>& it) const override {
        shuffle(it);
    }

    [[nodiscard]] std::string_view getName(void) const override {
        return "STD C++ pesudo RNG";
    }
    ~StdCpp() override = default;
};

#ifdef RDRAND_MAYBE_SUPPORTED
struct RDRand : Random::ImplBase {
    Random::ret_type generate(const Random::ret_type min,
                              const Random::ret_type max) const override {
        return gen_impl(&engine, min, max);
    }

    bool isSupported(void) const override { return rdrand_engine::supported(); }

    void shuffle(std::vector<std::string>& it) const override {
        ShuffleImpl(it, &engine);
    }

    [[nodiscard]] std::string_view getName() const override {
        return "X86 RDRAND instr. HWRNG (Intel/AMD)";
    }
    ~RDRand() override = default;

   private:
    rdrand_engine engine;
};
#endif

#ifdef KERNELRAND_MAYBE_SUPPORTED
struct KernelRand : Random::ImplBase {
    Random::ret_type generate(const Random::ret_type min,
                              const Random::ret_type max) const override {
        return gen_impl(&engine, min, max);
    }

    bool isSupported() const override {
        return engine.isSupported();
    }

    template <typename T>
    void shuffle(std::vector<T>& in) const {
        ShuffleImpl(in, &engine);
    }

    void shuffle(std::vector<std::string>& it) const override {
        shuffle(it);
    }

    [[nodiscard]] std::string_view getName() const override {
        return "Linux/MacOS HWRNG interface";
    }
    ~KernelRand() override = default;

    mutable kernel_rand_engine engine;
};
#endif

Random::Random() {
    std::vector<std::unique_ptr<Random::ImplBase>> impl_list;

#ifdef RDRAND_MAYBE_SUPPORTED
    impl_list.emplace_back(std::make_unique<RDRand>());
#endif

#ifdef KERNELRAND_MAYBE_SUPPORTED
    impl_list.emplace_back(std::make_unique<KernelRand>());
#endif

    impl_list.emplace_back(std::make_unique<StdCpp>());

    for (auto& i : impl_list) {
        if (i->isSupported()) {
            impl_ = std::move(i);
            LOG(INFO) << "Using " << impl_->getName() << " as RNG impl";
            break;
        }
    }
}

void Random::shuffle(std::vector<std::string>& array) const {
    impl_->shuffle(array);
}

Random::ret_type Random::generate(const ret_type min, const ret_type max) const {
    return impl_->generate(min, max);
}
