#include <array>
#include <unistd.h>
#include <absl/log/log.h>

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

inline int pipe(pipe_t& fd) {
    return pipe(fd.data());
}