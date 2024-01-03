#pragma once

#if defined __APPLE__ || defined __linux__

#include <Logging.h>
#include <RuntimeException.h>
#include <fcntl.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstdint>
#include <mutex>
#include <stdexcept>

#define KERNELRAND_MAYBE_SUPPORTED

struct kernel_rand_engine {
    using result_type = uint32_t;

    static constexpr result_type min() { return 0; }
    static constexpr result_type max() { return UINT32_MAX; }

    result_type operator()() {
        result_type val;
        read(fd, &val, sizeof(val));
        return val;
    }
    kernel_rand_engine() {
        for (const auto& n : nodes) {
            fd = open(n.c_str(), O_RDONLY);
            if (fd < 0) {
                if (errno != ENOENT) {
                    PLOG_E("Opening hwrng device '%s'", n.c_str());
                }
            } else
                break;
        }
        if (fd < 0)
            throw std::runtime_error("Failed to open hwrng device file");
    }
    ~kernel_rand_engine() {
        if (fd > 0)
            close(fd);
    }

    static bool isSupported(void) {
        static bool kSupported = false;
        static std::once_flag once;

        std::call_once(once, [] {
            int ret;
            for (const auto& n : nodes) {
                ret = access(n.c_str(), R_OK);
                if (ret != 0) {
                    if (errno != ENOENT) {
                        PLOG_E("Accessing hwrng device '%s'", n.c_str());
                    }
                } else {
                    kSupported = true;
                }
            }
        });
        return kSupported;
    }

   private:
    static const inline std::array<std::string, 2> nodes = {
#ifdef __linux__
        // Linux 4.16 or below
        "/dev/hw_random",
        // Linux 4.17 or above
        "/dev/hwrng",
#elif defined __APPLE__
        "/dev/random",
        "/whatever"
#endif
    };
    int fd = -1;
};
#endif
