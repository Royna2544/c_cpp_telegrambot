#include "SocketPosix.hpp"

#include <absl/log/log.h>
#include <socket/TgBotSocket.h>
#include <sys/socket.h>
#include <unistd.h>

#include <SharedMalloc.hpp>
#include <SocketData.hpp>
#include <cstring>
#include <socket/selector/SelectorPosix.hpp>

#include "SocketBase.hpp"

void SocketInterfaceUnix::startListening(socket_handle_t handle,
                                         const listener_callback_t onNewData) {
    bool should_break = false;
    int rc = 0;
    PollSelector selector;

    if (!isValidSocketHandle(handle)) {
        return;
    }

    do {
        if (listen(handle, SOMAXCONN) < 0) {
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
        selector.add(handle, [handle, this, &should_break, onNewData] {
            struct sockaddr addr {};
            socklen_t len = sizeof(addr);
            socket_handle_t cfd = accept(handle, &addr, &len);

            if (!isValidSocketHandle(cfd)) {
                PLOG(ERROR) << "Accept failed";
            } else {
                doGetRemoteAddr(cfd);
                SocketConnContext ctx(addr);
                ctx.cfd = cfd;
                should_break = onNewData(ctx);
                closeSocketHandle(cfd);
            }
        });
        while (!should_break) {
            switch (selector.poll()) {
                case Selector::SelectorPollResult::FAILED:
                    should_break = true;
                    break;
                case Selector::SelectorPollResult::OK:
                    break;
            }
        }
        selector.shutdown();
    } while (false);
    closePipe(kListenTerminate);
    closeSocketHandle(handle);
    cleanupServerSocket();
}

void SocketInterfaceUnix::forceStopListening() {
    if (isValidFd(notify_fd) && isValidSocketHandle(listen_fd)) {
        dummy_listen_buf_t d = {};
        ssize_t count = 0;

        count = write(notify_fd, &d, sizeof(dummy_listen_buf_t));
        if (count < 0) {
            PLOG(ERROR) << "Failed to write to notify pipe";
        }
        closeSocketHandle(notify_fd);
    }
}

char* SocketInterfaceUnix::getLastErrorMessage() { return strerror(errno); }

void SocketInterfaceUnix::writeToSocket(SocketConnContext context,
                                        SocketData data) {
    auto* socketData = static_cast<char*>(data.data->getData());
    auto* addr = static_cast<sockaddr*>(context.addr.getData());
    if (isValidSocketHandle(context.cfd)) {
        const auto count =
            send(context.cfd, socketData, data.len, MSG_NOSIGNAL);
        if (count < 0) {
            PLOG(ERROR) << "Failed to send to socket";
        }
    } else {
        const auto count = sendto(context.cfd, socketData, data.len,
                                  MSG_NOSIGNAL, addr, context.len);
        if (count < 0) {
            PLOG(ERROR) << "Failed to sentto socket";
        }
    }
}

std::optional<SocketData> SocketInterfaceUnix::readFromSocket(
    SocketConnContext handle, SocketData::length_type length) {
    SocketData buf(length);
    auto* addr = static_cast<sockaddr*>(handle.addr.getData());

    ssize_t count = recvfrom(handle.cfd, buf.data->getData(), length,
                             MSG_NOSIGNAL | MSG_WAITALL, addr, &handle.len);
    if (count < 0) {
        PLOG(ERROR) << "Failed to read from socket";
    } else {
        buf.len = count;
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