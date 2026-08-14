#pragma once

#if defined __APPLE__ || defined __linux__

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>

#ifdef __APPLE__
#include <stdlib.h>
#elif defined __linux__
#include <fcntl.h>
#include <sys/random.h>
#include <unistd.h>
#endif

#define KERNELRAND_MAYBE_SUPPORTED

// Bounded access to the platform CSPRNG. This engine is used only to seed a
// thread-local PRNG; generate() and shuffle() never invoke it directly.
struct kernel_rand_engine {
    using result_type = std::uint32_t;

    static constexpr result_type min() {
        return std::numeric_limits<result_type>::min();
    }
    static constexpr result_type max() {
        return std::numeric_limits<result_type>::max();
    }

    static bool supported() noexcept {
        static const bool kSupported = [] {
            result_type probe = 0;
            return fill(&probe, 1);
        }();
        return kSupported;
    }

    static bool fill(result_type* output, const std::size_t count) noexcept {
        if (output == nullptr || count == 0) {
            return count == 0;
        }
        if (count >
            std::numeric_limits<std::size_t>::max() / sizeof(result_type)) {
            return false;
        }

#ifdef __APPLE__
        arc4random_buf(output, count * sizeof(result_type));
        return true;
#elif defined __linux__
        auto* bytes = reinterpret_cast<unsigned char*>(output);
        const std::size_t size = count * sizeof(result_type);
        std::size_t offset = 0;
        unsigned int attempts = 0;
        constexpr unsigned int kMaxAttempts = 8;

        while (offset < size && attempts++ < kMaxAttempts) {
            const auto readCount =
                ::getrandom(bytes + offset, size - offset, GRND_NONBLOCK);
            if (readCount > 0) {
                offset += static_cast<std::size_t>(readCount);
                continue;
            }
            if (readCount < 0 && errno == EINTR) {
                continue;
            }
            break;
        }
        if (offset == size) {
            return true;
        }

        // getrandom can be unavailable on an old kernel, or return EAGAIN
        // during very early boot. /dev/urandom is the non-blocking OS CSPRNG
        // fallback; raw hardware RNG device nodes are deliberately not used.
        int flags = O_RDONLY | O_NONBLOCK;
#ifdef O_CLOEXEC
        flags |= O_CLOEXEC;
#endif
        const int fd = ::open("/dev/urandom", flags);
        if (fd < 0) {
            return false;
        }

        offset = 0;
        attempts = 0;
        while (offset < size && attempts++ < kMaxAttempts) {
            const auto readCount = ::read(fd, bytes + offset, size - offset);
            if (readCount > 0) {
                offset += static_cast<std::size_t>(readCount);
                continue;
            }
            if (readCount < 0 && errno == EINTR) {
                continue;
            }
            break;
        }
        ::close(fd);
        return offset == size;
#endif
    }
};
#endif
