// clang-format off
#include <winsock2.h>
#include <winsock.h>
#include <afunix.h>
// clang-format on
#include <impl/SocketWindows.hpp>
#include <socket/selector/SelectorWindows.hpp>

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
    const listener_callback_t onNewData) {
    bool should_break = false;
    WSADATA data;
    SelectSelector selector;
    socket_handle_t sfd = 0;

    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        WSALOG_E("WSAStartup failed");
    }

    sfd = createServerSocket();
    if (!isValidSocketHandle(sfd)) {
        return;
    }

    do {
        if (listen(sfd, SOMAXCONN) == SOCKET_ERROR) {
            WSALOG_E("Failed to listen to socket");
            break;
        }
        if (!selector.init()) {
            break;
        }
        selector.add(sfd, [sfd, this, &should_break, onNewData] {
            struct sockaddr addr {};
            socklen_t len = sizeof(addr);
            const socket_handle_t cfd = accept(sfd, &addr, &len);

            if (!isValidSocketHandle(cfd)) {
                WSALOG_E("Accept failed");
            } else {
                doGetRemoteAddr(cfd);
                should_break = onNewData(this, cfd);
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
    closesocket(sfd);
    cleanupServerSocket();
    WSACleanup();
}

void SocketInterfaceWindows::writeToSocket(struct SocketData sdata) {
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) == 0) {
        const socket_handle_t sfd = createClientSocket();
        if (isValidSocketHandle(sfd)) {
            const int count =
                send(sfd, reinterpret_cast<char *>(sdata.data->getData()),
                     sdata.len, 0);
            if (count < 0) {
                WSALOG_E("Failed to send to socket");
            }
            closesocket(sfd);
        }
    }
    WSACleanup();
}

void SocketInterfaceWindows::forceStopListening(void) { kRun = false; }

char *SocketInterfaceWindows::getLastErrorMessage() {
    return strWSAError(WSAGetLastError());
}

std::optional<SocketData> SocketInterfaceWindows::readFromSocket(
    socket_handle_t handle, SocketData::length_type length) {
    SocketData buf(length);
    int count =
        recv(handle, static_cast<char *>(buf.data->getData()), length, 0);
    if (count < 0) {
        PLOG(ERROR) << "Failed to read from socket";
    } else {
        buf.len = count;
        return buf;
    }
    return std::nullopt;
}

void WSALOG_E(const char *msg) {
    LOG(ERROR) << msg << ": "
               << SocketInterfaceWindows::strWSAError(WSAGetLastError());
}