#pragma once

#if defined __APPLE__ || defined __linux__

#include <absl/log/check.h>
#include <absl/log/log.h>

#include <array>
#include <cerrno>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <mutex>

#define KERNELRAND_MAYBE_SUPPORTED

struct kernel_rand_engine {
    using result_type = uint32_t;

    static constexpr result_type min() {
        return std::numeric_limits<result_type>::min();
    }
    static constexpr result_type max() {
        return std::numeric_limits<result_type>::max();
    }

    result_type operator()() const noexcept { return read().value_or(0); }

    kernel_rand_engine() { isSupported(); }

    bool isSupported() {
        static bool kSupported = [this] {
            for (const auto& n : nodes) {
                rng.open(n.data(), std::ios::in | std::ios::binary);
                if (rng) {
                    if (read()) {
                        LOG(INFO) << "Device ready, Node: " << std::quoted(n);
                        return true;
                    }
                } else {
                    PLOG(ERROR)
                        << "Opening node " << std::quoted(n) << " failed";
                }
            }
            return false;
        }();
        return kSupported;
    }

   private:
    std::optional<result_type> read() const noexcept {
        result_type val;

        if (rng.read(reinterpret_cast<char*>(&val), sizeof(val)); !rng) {
            PLOG(ERROR) << "Failed to read data from HWRNG device";
            rng.clear();
            return std::nullopt;
        }
        return val;
    }

    static const inline std::array<std::string_view, 2> nodes = {
#ifdef __linux__
        // Linux 4.16 or below
        "/dev/hw_random",
        // Linux 4.17 or above
        "/dev/hwrng",
#elif defined __APPLE__
        "/dev/random", "/dev/urandom"
#endif
    };
    mutable std::ifstream rng;
};
#endif
