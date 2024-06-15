#include <absl/log/log.h>
#include <unistd.h>

#include <array>

using pipe_t = std::array<int, 2>;

constexpr int kInvalidFD = -1;

inline bool isValidFd(int fd) { return fd != kInvalidFD; }

inline void closeFd(int& fd) {
    int rc = 0;

    if (isValidFd(fd)) rc = close(fd);

    PLOG_IF(ERROR, rc != 0) << "Failed to close fd: " << fd;
    fd = kInvalidFD;
}

inline bool IsValidPipe(pipe_t fd) {
    return isValidFd(fd[0]) && isValidFd(fd[1]);
}

inline void InvaildatePipe(pipe_t fd) {
    fd[0] = kInvalidFD;
    fd[1] = kInvalidFD;
}

inline void closePipe(pipe_t fd) {
    closeFd(fd[0]);
    closeFd(fd[1]);
}

inline int pipe(pipe_t& fd) { return pipe(fd.data()); }

/**
 * @brief Creates a unique_ptr that automatically closes the given file descriptor when it goes out of scope.
 *
 * @param[in, out] fd A pointer to the file descriptor to be managed.
 *
 * @return A unique_ptr that owns the given file descriptor and will automatically close it when it goes out of scope.
 */
inline auto createFdAutoCloser(int* fd) {
    return std::unique_ptr<int, void (*)(int*)>(
        fd, [](int* _fd) { closeFd(*_fd); });
}