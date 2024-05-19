// clang-format off
#include <winsock2.h>
#include <winsock.h>
#include <afunix.h>
// clang-format on
#include <impl/SocketWindows.hpp>
#include <socket/selector/SelectorWindows.hpp>

#include "SharedMalloc.hpp"
#include "SocketBase.hpp"

char *SocketInterfaceWindows::strWSAError(const int errcode) {
    int ret = 0;
    static char strerror_buf[64];

#define MAP_WSA_TO_POSIX(val, posix) \
    case WSA##posix:                 \
        val = posix;                 \
        break;

    switch (errcode) {
        MAP_WSA_TO_POSIX(ret, EINTR);
        MAP_WSA_TO_POSIX(ret, EACCES);
        MAP_WSA_TO_POSIX(ret, EFAULT);
        MAP_WSA_TO_POSIX(ret, EINVAL);
        MAP_WSA_TO_POSIX(ret, EMFILE);
        MAP_WSA_TO_POSIX(ret, EWOULDBLOCK);
        MAP_WSA_TO_POSIX(ret, EINPROGRESS);
        MAP_WSA_TO_POSIX(ret, EALREADY);
        MAP_WSA_TO_POSIX(ret, ENOTSOCK);
        MAP_WSA_TO_POSIX(ret, EDESTADDRREQ);
        MAP_WSA_TO_POSIX(ret, EMSGSIZE);
        MAP_WSA_TO_POSIX(ret, EPROTOTYPE);
        MAP_WSA_TO_POSIX(ret, ENOPROTOOPT);
        MAP_WSA_TO_POSIX(ret, EPROTONOSUPPORT);
        MAP_WSA_TO_POSIX(ret, EOPNOTSUPP);
        MAP_WSA_TO_POSIX(ret, EAFNOSUPPORT);
        MAP_WSA_TO_POSIX(ret, EADDRINUSE);
        MAP_WSA_TO_POSIX(ret, EADDRNOTAVAIL);
        MAP_WSA_TO_POSIX(ret, ENETDOWN);
        MAP_WSA_TO_POSIX(ret, ENETUNREACH);
        MAP_WSA_TO_POSIX(ret, ENETRESET);
        MAP_WSA_TO_POSIX(ret, ECONNABORTED);
        MAP_WSA_TO_POSIX(ret, ECONNRESET);
        MAP_WSA_TO_POSIX(ret, ENOBUFS);
        MAP_WSA_TO_POSIX(ret, EISCONN);
        MAP_WSA_TO_POSIX(ret, ENOTCONN);
        MAP_WSA_TO_POSIX(ret, ETIMEDOUT);
        MAP_WSA_TO_POSIX(ret, ECONNREFUSED);
        MAP_WSA_TO_POSIX(ret, ELOOP);
        MAP_WSA_TO_POSIX(ret, ENAMETOOLONG);
        MAP_WSA_TO_POSIX(ret, EHOSTUNREACH);
        MAP_WSA_TO_POSIX(ret, ENOTEMPTY);
        default:
            memset(strerror_buf, 0, sizeof(strerror_buf));
            snprintf(strerror_buf, sizeof(strerror_buf), "code: %d", errcode);
            return strerror_buf;
    }
    return strerror(ret);
}

void SocketInterfaceWindows::startListening(
    socket_handle_t handle, const listener_callback_t onNewData) {
    bool should_break = false;
    WSADATA data;
    SelectSelector selector;

    do {
        if (listen(handle, SOMAXCONN) == SOCKET_ERROR) {
            WSALOG_E("Failed to listen to socket");
            break;
        }
        if (!selector.init()) {
            break;
        }
        selector.add(handle, [handle, this, &should_break, onNewData] {
            struct sockaddr addr {};
            socklen_t len = sizeof(addr);
            const socket_handle_t cfd = accept(handle, &addr, &len);

            if (!isValidSocketHandle(cfd)) {
                WSALOG_E("Accept failed");
            } else {
                doGetRemoteAddr(cfd);
                SocketConnContext ctx(addr);
                ctx.cfd = cfd;
                should_break = onNewData(ctx);
                closesocket(cfd);
            }
        });
        while (!should_break && kRun) {
            switch (selector.poll()) {
                case Selector::SelectorPollResult::FAILED:
                    LOG(ERROR) << "Poll failed";
                    should_break = true;
                    break;
                case Selector::SelectorPollResult::OK:
                    break;
            }
        }
        selector.shutdown();
    } while (false);
    closeSocketHandle(handle);
    cleanupServerSocket();
}

void SocketInterfaceWindows::writeToSocket(SocketConnContext context,
                                           SharedMalloc data) {
    auto *socketData = static_cast<char *>(data.get());
    auto *addr = static_cast<sockaddr *>(context.addr.get());
    if (isValidSocketHandle(context.cfd)) {
        const auto count = send(context.cfd, socketData, data->size, 0);
        if (count < 0) {
            WSALOG_E("Failed to send to socket");
        }
    } else {
        const auto count =
            sendto(context.cfd, socketData, data->size, 0, addr, context.len);
        if (count < 0) {
            WSALOG_E("Failed to sentto socket");
        }
    }
}

void SocketInterfaceWindows::forceStopListening(void) { kRun = false; }

char *SocketInterfaceWindows::getLastErrorMessage() {
    return strWSAError(WSAGetLastError());
}

std::optional<SharedMalloc> SocketInterfaceWindows::readFromSocket(
    SocketConnContext context, TgBotCommandPacketHeader::length_type length) {
    SharedMalloc buf(length);
    auto *addr = static_cast<sockaddr *>(context.addr.get());
    auto *data = static_cast<char *>(buf.get());

    auto count =
        recvfrom(context.cfd, data, length, MSG_WAITALL, addr, &context.len);
    if (count != length) {
        WSALOG_E("Failed to read from socket");
    } else {
        return buf;
    }
    return std::nullopt;
}

bool SocketInterfaceWindows::closeSocketHandle(socket_handle_t &handle) {
    if (isValidSocketHandle(handle)) {
        closesocket(handle);
        handle = INVALID_SOCKET;
        return true;
    }
    return false;
}

void WSALOG_E(const char *msg) {
    LOG(ERROR) << msg << ": "
               << SocketInterfaceWindows::strWSAError(WSAGetLastError());
}