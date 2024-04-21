#include "SocketPosix.hpp"

#include <absl/log/log.h>
#include <socket/TgBotSocket.h>
#include <sys/socket.h>
#include <unistd.h>

#include <SharedMalloc.hpp>
#include <SocketData.hpp>
#include <cstring>
#include <socket/selector/SelectorPosix.hpp>

void SocketInterfaceUnix::startListening(const listener_callback_t onNewData) {
    bool should_break = false;
    int rc = 0;
    socket_handle_t sfd = createServerSocket();
    PollSelector selector;
    if (isValidSocketHandle(sfd)) {
        do {
            if (listen(sfd, SOMAXCONN) < 0) {
                PLOG(ERROR) << "Failed to listen to socket";
                break;
            }
            if (pipe(kListenTerminate) != 0) {
                PLOG(ERROR) << "Failed to create pipe";
                break;
            }
            if (!selector.init()) {
                break;
            }
            selector.add(listen_fd, [this, &should_break]() {
                dummy_listen_buf_t buf = {};
                ssize_t rc = read(listen_fd, &buf, sizeof(dummy_listen_buf_t));
                if (rc < 0) {
                    PLOG(ERROR) << "Reading data from forcestop fd";
                }
                closeFd(listen_fd);
                should_break = true;
            });
            selector.add(sfd, [sfd, this, &should_break, onNewData] {
                struct sockaddr addr {};
                socklen_t len = sizeof(addr);
                const socket_handle_t cfd = accept(sfd, &addr, &len);

                if (!isValidSocketHandle(cfd)) {
                    PLOG(ERROR) << "Accept failed";
                } else {
                    doGetRemoteAddr(cfd);
                    should_break = onNewData(this, cfd);
                    close(cfd);
                }
            });
            while (!should_break) {
                selector.poll();
            }
            selector.shutdown();
        } while (false);
        closePipe(kListenTerminate);
        close(sfd);
        cleanupServerSocket();
    }
}

void SocketInterfaceUnix::writeToSocket(struct SocketData data) {
    const socket_handle_t sfd = createClientSocket();
    if (isValidSocketHandle(sfd)) {
        const ssize_t count = write(sfd, data.data->getData(), data.len);
        if (count < 0) {
            PLOG(ERROR) << "Failed to write to socket";
        }
        close(sfd);
    }
}

void SocketInterfaceUnix::forceStopListening() {
    if (isValidFd(notify_fd) && isValidSocketHandle(listen_fd)) {
        dummy_listen_buf_t d = {};
        ssize_t count = 0;

        count = write(notify_fd, &d, sizeof(dummy_listen_buf_t));
        if (count < 0) {
            PLOG(ERROR) << "Failed to write to notify pipe";
        }
        closeFd(notify_fd);
    }
}

char* SocketInterfaceUnix::getLastErrorMessage() { return strerror(errno); }

std::optional<SocketData> SocketInterfaceUnix::readFromSocket(
    socket_handle_t handle, SocketData::length_type length) {
    SocketData buf(length);
    ssize_t count = read(handle, buf.data->getData(), length);
    if (count < 0) {
        PLOG(ERROR) << "Failed to read from socket";
    } else {
        buf.len = count;
        return buf;
    }
    return std::nullopt;
}
