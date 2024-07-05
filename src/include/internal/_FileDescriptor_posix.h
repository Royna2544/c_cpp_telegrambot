#include <absl/log/log.h>
#include <unistd.h>

#include <array>
#include <cerrno>

using pipe_t = std::array<int, 2>;

constexpr int kInvalidFD = -1;

inline bool isValidFd(int fd) { return fd != kInvalidFD; }

inline void closeFd(int& fd) {
    int rc = 0;

    if (isValidFd(fd)) {
        rc = close(fd);
    }

    PLOG_IF(ERROR, (rc != 0 && errno != EBADF))
        << "Failed to close fd: " << fd;
    fd = kInvalidFD;
}

/**
 * @brief A class representing a pipe with two file descriptors.
 *
 * This class represents a pipe with two file descriptors, one for reading and
 * one for writing. It provides methods to check if the pipe is valid,
 * invalidate the pipe, close the pipe, and get the read and write file
 * descriptors.
 */
struct Pipe {
    /**
     * @brief Enum representing the two endpoints of a pipe.
     *
     * This enum represents the two endpoints of a pipe, READ and WRITE.
     */
    enum class EndPoint {
        READ = 0,
        WRITE = 1,
    };

    /**
     * @brief Checks if the pipe is valid.
     *
     * This method checks if the pipe is valid by verifying if both file
     * descriptors are valid.
     *
     * @return True if the pipe is valid, false otherwise.
     */
    [[nodiscard]] bool isVaild() const {
        return isValidFd(underlying[0]) && isValidFd(underlying[1]);
    }

    /**
     * @brief Invalidates the pipe.
     *
     * This method invalidates the pipe by setting both file descriptors to an
     * invalid value.
     */
    void invalidate() {
        underlying[0] = kInvalidFD;
        underlying[1] = kInvalidFD;
    }

    /**
     * @brief Closes the pipe.
     *
     * This method closes the pipe by calling the closeFd method on both file
     * descriptors.
     */
    void close() {
        closeFd(underlying[0]);
        closeFd(underlying[1]);
    }

    /**
     * @brief Creates a pipe.
     *
     * This method creates a pipe by calling the pipe system call and stores the
     * file descriptors in the underlying array.
     *
     * @return True if the pipe is successfully created, false otherwise.
     */
    bool pipe() {
        int rc = ::pipe(underlying.data());
        PLOG_IF(ERROR, rc != 0) << "Failed to create pipe";
        return rc == 0;
    }

    int operator[](EndPoint ep) const {
        return underlying[static_cast<size_t>(ep)];
    }

    /**
     * @brief Gets the read end file descriptor of the pipe.
     *
     * This method returns the file descriptor for reading from the pipe.
     *
     * @return The file descriptor for reading from the pipe.
     */
    [[nodiscard]] int readEnd() const { return operator[](EndPoint::READ); }

    /**
     * @brief Gets the write end file descriptor of the pipe.
     *
     * This method returns the file descriptor for writing to the pipe.
     *
     * @return The file descriptor for writing to the pipe.
     */
    [[nodiscard]] int writeEnd() const { return operator[](EndPoint::WRITE); }

    /**
     * @brief The underlying array of file descriptors.
     */
    pipe_t underlying;
};

/**
 * @brief Creates a unique_ptr that automatically closes the given file
 * descriptor when it goes out of scope.
 *
 * @param[in, out] fd A pointer to the file descriptor to be managed.
 *
 * @return A unique_ptr that owns the given file descriptor and will
 * automatically close it when it goes out of scope.
 */
inline auto createFdAutoCloser(int* fd) {
    return std::unique_ptr<int, void (*)(int*)>(
        fd, [](int* _fd) { closeFd(*_fd); });
}