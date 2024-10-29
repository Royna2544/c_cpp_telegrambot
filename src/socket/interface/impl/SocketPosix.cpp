#include "SocketPosix.hpp"

#include <absl/log/log.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <socket/selector/SelectorPosix.hpp>

#include "socket/selector/Selectors.hpp"

void SocketInterfaceUnix::startListening(socket_handle_t handle,
                                         const listener_callback_t onNewData) {
    int rc = 0;
    UnixSelector selector;

    if (!isValidSocketHandle(handle)) {
        return;
    }

    do {
        if (listen(handle, SOMAXCONN) < 0) {
            PLOG(ERROR) << "Failed to listen to socket";
            break;
        }
        if (!kListenTerminate.pipe()) {
            PLOG(ERROR) << "Failed to create pipe";
            break;
        }
        if (!selector.init()) {
            break;
        }
        selector.add(
            kListenTerminate.readEnd(),
            [this]() {
                dummy_listen_buf_t buf = {};
                ssize_t rc = read(kListenTerminate.readEnd(), &buf,
                                  sizeof(dummy_listen_buf_t));
                if (rc < 0) {
                    PLOG(ERROR) << "Reading data from forcestop fd";
                }
                kShouldRun = false;
                closeFd(kListenTerminate.readEnd());
            },
            Selector::Mode::READ);
        selector.add(
            handle,
            [handle, this, onNewData] {
                struct sockaddr addr {};
                socklen_t len = sizeof(addr);
                socket_handle_t cfd = accept(handle, &addr, &len);

                if (!isValidSocketHandle(cfd)) {
                    PLOG(ERROR) << "Accept failed";
                } else {
                    printRemoteAddress(cfd);
                    setSocketOptTimeout(cfd, 5);
                    SocketConnContext ctx(cfd, addr);
                    kShouldRun = onNewData(ctx);
                    closeSocketHandle(cfd);
                }
            },
            Selector::Mode::READ);
        sInitialized = true;
        while (kShouldRun) {
            switch (selector.poll()) {
                case Selector::PollResult::FAILED:
                    kShouldRun = false;
                    break;
                case Selector::PollResult::OK:
                case Selector::PollResult::TIMEOUT:
                    break;
            }
        }
        selector.shutdown();
    } while (false);
    kListenTerminate.close();
    closeSocketHandle(handle);
    cleanupServerSocket();
}

bool SocketInterfaceUnix::forceStopListening() {
    if (!sInitialized) {
        LOG(INFO) << "Initialized=false, putting kShouldRun=false";
        kShouldRun = false;
        return true;
    }
    if (kListenTerminate.isVaild()) {
        dummy_listen_buf_t d = {};
        ssize_t count = 0;
        int notify_fd = kListenTerminate.writeEnd();

        count = write(notify_fd, &d, sizeof(dummy_listen_buf_t));
        if (count < 0) {
            PLOG(ERROR) << "Failed to write to notify pipe";
        }
        closeFd(notify_fd);
        return true;
    } else {
        LOG(ERROR) << "Force stop listening not possible...";
        LOG(ERROR) << kListenTerminate;
        return false;
    }
}

namespace libc_shim {

bool should_retry() { return errno == EINTR || errno == EWOULDBLOCK; }

bool recv(int fd, void* buf, size_t n, int flags) {
    ssize_t remaining = n;
    ssize_t rc = 0;
    while (remaining > 0) {
        rc = ::recv(fd, buf, remaining, flags);
        DLOG(INFO) << "recv: rc=" << rc << " remaining=" << remaining
                   << " now-remaining=" << remaining - rc;
        if (rc < 0) {
            if (should_retry()) {
                continue;
            } else {
                PLOG(ERROR) << "Failed to receive from socket";
                return false;
            }
        } else if (rc == 0) {
            LOG(WARNING) << "Connection aborted";
            return false;
        }
        remaining -= rc;
        buf = static_cast<char*>(buf) + rc;
    }
    return true;
}

bool send(int fd, const void* buf, size_t n, int flags) {
    ssize_t remaining = n;
    ssize_t rc = 0;
    while (remaining > 0) {
        rc = ::send(fd, buf, remaining, flags);
        DLOG(INFO) << "send: rc=" << rc << " remaining=" << remaining
                   << " now-remaining=" << remaining - rc;
        if (rc < 0) {
            if (should_retry()) {
                continue;
            } else {
                PLOG(ERROR) << "Failed to send to socket";
                return false;
            }
        } else if (rc == 0) {
            LOG(WARNING) << "Connection aborted";
            return false;
        }
        remaining -= rc;
        buf = static_cast<const char*>(buf) + rc;
    }
    return true;
}

template <typename... Args>
bool sendto(Args&&... args) {
    ssize_t rc = ::sendto(std::forward<Args>(args)...);
    if (rc < 0) {
        PLOG(ERROR) << "Failed to sendto to socket";
        return false;
    }
    return true;
}

template <typename... Args>
bool recvfrom(Args&&... args) {
    ssize_t rc = ::recvfrom(std::forward<Args>(args)...);
    if (rc < 0) {
        PLOG(ERROR) << "Failed to recvfrom from socket";
        return false;
    }
    return true;
}

}  // namespace libc_shim

bool SocketInterfaceUnix::writeToSocket(SocketConnContext context,
                                        SharedMalloc data) {
    auto* socketData = static_cast<char*>(data.get());
    auto* addr = static_cast<sockaddr*>(context.addr.get());
    bool use_udp = options.use_udp.get();
    constexpr int kSendFlags = MSG_NOSIGNAL;

    if (use_udp) {
        return libc_shim::sendto(context.cfd, socketData, data->size(),
                                 kSendFlags, addr, context.addr->size());
    } else {
        return libc_shim::send(context.cfd, socketData, data->size(),
                               kSendFlags);
    }
}

std::optional<SharedMalloc> SocketInterfaceUnix::readFromSocket(
    SocketConnContext handle, buffer_len_t length) {
    SharedMalloc buf(length);
    auto* addr = static_cast<sockaddr*>(handle.addr.get());
    socklen_t addrlen = handle.addr->size();
    ssize_t count = 0;
    constexpr int kRecvFlags = MSG_NOSIGNAL;
    bool sent = false;

    if (options.use_udp.get()) {
        sent = libc_shim::recvfrom(handle.cfd, buf.get(), length, kRecvFlags,
                                   addr, &addrlen);
    } else {
        sent = libc_shim::recv(handle.cfd, buf.get(), length, kRecvFlags);
    }
    if (sent) {
        return buf;
    }
    return std::nullopt;
}

bool SocketInterfaceUnix::closeSocketHandle(socket_handle_t& handle) {
    if (isValidSocketHandle(handle)) {
        closeFd(handle);
        handle = kInvalidFD;
        return true;
    }
    return false;
}

bool SocketInterfaceUnix::setSocketOptTimeout(socket_handle_t handle,
                                              int timeout) {
    struct timeval tv {
        .tv_sec = timeout
    };
    int ret = 0;

    ret |= setsockopt(handle, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    ret |= setsockopt(handle, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    if (ret) {
        PLOG(ERROR) << "Failed to set socket timeout";
    }
    return ret == 0;
}
