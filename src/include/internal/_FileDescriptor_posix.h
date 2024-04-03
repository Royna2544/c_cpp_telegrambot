#include <unistd.h>
#include <absl/log/log.h>

using pipe_t = int[2];

constexpr int kInvalidFD = -1;

static inline bool isValidFd(int fd) { return fd != kInvalidFD; }

static inline void closeFd(int& fd) {
    int rc = 0;

    if (isValidFd(fd)) rc = close(fd);

    PLOG_IF(ERROR, rc != 0) << "Failed to close fd: " << fd;
    fd = kInvalidFD;
}

static inline bool IsValidPipe(pipe_t fd) {
    return isValidFd(fd[0]) && isValidFd(fd[1]);
}

static inline void InvaildatePipe(pipe_t fd) {
    fd[0] = kInvalidFD;
    fd[1] = kInvalidFD;
}

static inline void closePipe(pipe_t fd) {
    closeFd(fd[0]);
    closeFd(fd[1]);
}
