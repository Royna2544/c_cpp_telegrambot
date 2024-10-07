#pragma once

#include <absl/log/check.h>
#include <absl/log/log.h>
#include <sys/types.h>

#include <cstring>
#include <sstream>
#include <string>
#include <string_view>
#include "internal/_class_helper_macros.h"

class syscall_perror : public std::exception {
   public:
    explicit syscall_perror(const std::string_view syscall, int _errno = errno)
        : _errno(_errno) {
        std::stringstream ss;
        ss << "Error in " << syscall << ": " << strerror(_errno);
        message = ss.str();
    }

    const char* what() const noexcept override { return message.c_str(); }
    [[nodiscard]] int error_code() const noexcept { return _errno; }

   private:
    std::string message;
    int _errno;
};

struct Shmem {
    std::string_view path;
    off_t size;
    void* memory;

    template <typename T>
    T* get() {
        CHECK_EQ(sizeof(T), size) << "size mismatch on get()";
        return static_cast<T*>(memory);
    }
};

struct AllocatedShmem : Shmem {
    /**
     * @brief Constructs a new shared memory segment and maps it into the
     * calling process' address space.
     *
     * This constructor creates a new shared memory segment identified by the
     * given path and size. If a shared memory segment with the same path
     * already exists, it is removed before creating a new one. The shared
     * memory segment is then mapped into the calling process' address space.
     *
     * @param path The path of the shared memory segment.
     * @param size The size of the shared memory segment in bytes.
     *
     * @throws syscall_perror If an error occurs during the creation or mapping
     * of the shared memory segment.
     *
     * @return A new AllocatedShmem object representing the created and mapped
     * shared memory segment.
     */
    explicit AllocatedShmem(const std::string_view& path, off_t size);

    AllocatedShmem() = default;
    ~AllocatedShmem();

    NO_COPY_CTOR(AllocatedShmem);
    NO_MOVE_CTOR(AllocatedShmem);

    template <typename T>
    T* get() {
        return Shmem::get<T>();
    }
};

struct ConnectedShmem : Shmem {
    /**
     * @brief Constructs a new shared memory segment and connects it to the
     * existing shared memory segment identified by the given path.
     *
     * This constructor connects to an existing shared memory segment identified
     * by the given path. If the shared memory segment does not exist, it is
     * created with the specified size.
     *
     * @param path The path of the shared memory segment.
     * @param size The size of the shared memory segment in bytes.
     *
     * @throws syscall_perror If an error occurs during the connection or
     * creation of the shared memory segment.
     *
     * @return A new ConnectedShmem object representing the connected shared
     * memory segment.
     *
     */
    explicit ConnectedShmem(const std::string_view& path, const off_t size);

    ConnectedShmem() = default;
    ~ConnectedShmem();

    NO_COPY_CTOR(ConnectedShmem);
    NO_MOVE_CTOR(ConnectedShmem);

    template <typename T>
    T* get() {
        return Shmem::get<T>();
    }
};
