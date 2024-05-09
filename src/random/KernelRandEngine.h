#pragma once

#if defined __APPLE__ || defined __linux__

#include <absl/log/check.h>
#include <absl/log/log.h>
#include <fcntl.h>
#include <internal/_FileDescriptor_posix.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstdint>
#include <mutex>

#define KERNELRAND_MAYBE_SUPPORTED

struct kernel_rand_engine {
    using result_type = uint32_t;

    static constexpr result_type min() { return 0; }
    static constexpr result_type max() { return UINT32_MAX; }

    result_type operator()() const {
        result_type val = 0;
        ssize_t rc;

        rc = read(fd, &val, sizeof(val));
        if (rc < 0)
            PLOG(ERROR) << "Failed to read data from HWRNG device: fd " << fd;
        return val;
    }
    kernel_rand_engine() {
        for (const auto& n : nodes) {
            fd = open(n.c_str(), O_RDONLY);
            if (!isValidFd(fd) && errno != ENOENT) {
                PLOG(ERROR) << "Opening hwrng device failed";
            } else
                break;
        }
        CHECK(isValidFd(fd));
    }
    ~kernel_rand_engine() { closeFd(fd); }

    static bool isSupported(void) {
        static bool kSupported = false;
        static std::once_flag once;

        std::call_once(once, [] {
            int ret = 0;
            for (const auto& n : nodes) {
                ret = access(n.c_str(), R_OK);
                if (ret != 0) {
                    if (errno != ENOENT) {
                        PLOG(ERROR) << "Accessing hwrng device failed";
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
        "/dev/random", "/dev/urandom"
#endif
    };
    int fd = kInvalidFD;
};
#endif
