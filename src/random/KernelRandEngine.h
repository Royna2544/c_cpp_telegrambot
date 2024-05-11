#pragma once

#if defined __APPLE__ || defined __linux__

#include <absl/log/check.h>
#include <absl/log/log.h>
#include <fcntl.h>
#include <internal/_FileDescriptor_posix.h>
#include <unistd.h>

#include <InstanceClassBase.hpp>
#include <array>
#include <cerrno>
#include <cstdint>
#include <mutex>

#define KERNELRAND_MAYBE_SUPPORTED

struct kernel_rand_engine : InstanceClassBase<kernel_rand_engine> {
    using result_type = uint32_t;

    static constexpr result_type min() { return 0; }
    static constexpr result_type max() { return UINT32_MAX; }

    result_type operator()() const {
        result_type val = 0;
        ssize_t rc = 0;

        rc = read(fd, &val, sizeof(val));
        if (rc < 0) PLOG(ERROR) << "Failed to read data from HWRNG device";
        return val;
    }
    kernel_rand_engine() { isSupported(); }
    ~kernel_rand_engine() { closeFd(fd); }

    bool isSupported(void) {
        static bool kSupported = false;
        static std::once_flag once;

        std::call_once(once, [this] {
            int ret = 0;
            for (const auto& n : nodes) {
                ret = access(n.c_str(), R_OK);
                if (ret != 0) {
                    if (errno != ENOENT) {
                        PLOG(ERROR) << "Accessing hwrng device failed";
                    }
                } else {
                    fd = open(n.c_str(), O_RDONLY);
                    if (!isValidFd(fd)) {
                        PLOG(ERROR) << "Opening hwrng device failed";
                        kSupported = false;
                    } else {
                        // Test read some bytes
                        int data = 0;
                        ret = read(fd, &data, sizeof(data));
                        if (ret != sizeof(data)) {
                            PLOG(ERROR) << "Reading from hwrng device failed";
                            kSupported = false;
                        } else {
                            LOG(INFO)
                                << "Device ready, Node: " << std::quoted(n)
                                << " fd: " << fd;
                            kSupported = true;
                        }
                    }
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
