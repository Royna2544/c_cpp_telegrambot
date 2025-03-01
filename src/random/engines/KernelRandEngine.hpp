#pragma once

#if defined __APPLE__ || defined __linux__

#include <absl/log/check.h>
#include <absl/log/log.h>
#include <fcntl.h>
#include <trivial_helpers/_FileDescriptor_posix.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstdint>
#include <iomanip>
#include <mutex>

#define KERNELRAND_MAYBE_SUPPORTED

struct kernel_rand_engine {
    using result_type = uint32_t;

    static constexpr result_type min() { return 0; }
    static constexpr result_type max() { return UINT32_MAX; }

    result_type operator()() const {
        result_type val = 0;
        ssize_t rc = 0;

        rc = read(fd, &val, sizeof(val));
        PLOG_IF(ERROR, rc < 0) << "Failed to read data from HWRNG device";
        return val;
    }
    kernel_rand_engine() { isSupported(); }
    ~kernel_rand_engine() { closeFd(fd); }
    kernel_rand_engine(kernel_rand_engine&& other) noexcept : fd(other.fd) {
        other.fd = -1;
    }

    bool isSupported() {
        static bool kSupported = [this] {
            ssize_t ret = 0;
            for (const auto& n : nodes) {
                fd = open(n.data(), O_RDONLY);
                if (isValidFd(fd)) {
                    // Test read some bytes
                    int data = 0;
                    ret = read(fd, &data, sizeof(data));
                    if (ret != sizeof(data)) {
                        PLOG(ERROR) << "Reading from hwrng device failed";
                        closeFd(fd);
                    } else {
                        LOG(INFO) << "Device ready, Node: " << std::quoted(n)
                                  << " fd: " << fd;
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
    int fd = kInvalidFD;
};
#endif
