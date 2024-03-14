#include "SocketInterfaceWindows.h"

// clang-format off
#include <winsock2.h>
#include <winsock.h>
#include <afunix.h>
#include "Logging.h"
#include "socket/TgBotSocket.h"
// clang-format on

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
    const listener_callback_t &cb, std::promise<bool> &createdPromise) {
    bool should_break = false;
    struct fd_set set;

    const socket_handle_t sfd = createServerSocket();
    isRunning = true;

    if (isValidSocketHandle(sfd)) {
        do {
            if (listen(sfd, SOMAXCONN) == SOCKET_ERROR) {
                WSALOG_E("Failed to listen to socket");
                break;
            }
            createdPromise.set_value(true);
            while (!should_break) {
                struct sockaddr_un addr {};
                struct TgBotConnection conn {};
                struct timeval tv = {10, 0};
                int len = sizeof(addr);

                FD_ZERO(&set);
                FD_SET(sfd, &set);
                if (select(0, &set, NULL, NULL, &tv) == SOCKET_ERROR) {
                    WSALOG_E("Select failed");
                    break;
                }
                if (FD_ISSET(sfd, &set)) {
                    const socket_handle_t cfd =
                        accept(sfd, (struct sockaddr *)&addr, &len);
                    if (cfd == INVALID_SOCKET) {
                        WSALOG_E("Accept failed");
                        break;
                    } else {
                        LOG(LogLevel::DEBUG, "Client connected");
                    }
                    const int count = recv(cfd, reinterpret_cast<char *>(&conn),
                                           sizeof(conn), 0);
                    should_break = handleIncomingBuf(count, conn, cb, [] {
                        return strWSAError(WSAGetLastError());
                    });
                    closesocket(cfd);
                } else {
                    if (!kRun) {
                        LOG(LogLevel::DEBUG, "Exiting");
                        break;
                    }
                }
            }
        } while (false);
        closesocket(sfd);
        cleanupServerSocket();
        WSACleanup();
        isRunning = false;
        return;
    }
    createdPromise.set_value(false);
    isRunning = false;
}

void SocketInterfaceWindows::writeToSocket(struct TgBotConnection conn) {
    const socket_handle_t sfd = createClientSocket();
    if (isValidSocketHandle(sfd)) {
        const int count =
            send(sfd, reinterpret_cast<char *>(&conn), sizeof(conn), 0);
        if (count < 0) {
            WSALOG_E("Failed to send to socket");
        }
        closesocket(sfd);
        WSACleanup();
    }
}

void SocketInterfaceWindows::forceStopListening(void) { kRun = false; }

std::map<SocketUsage, std::shared_ptr<SocketInterfaceBase>> socket_interfaces{
    {SocketUsage::SU_INTERNAL, std::make_shared<SocketInterfaceWindowsLocal>()},
    {SocketUsage::SU_EXTERNAL, std::make_shared<SocketInterfaceWindowsIPv4>()},
};

std::vector<std::shared_ptr<SocketInterfaceBase>> socket_interfaces_client{
    std::make_shared<SocketInterfaceWindowsIPv4>(),
    std::make_shared<SocketInterfaceWindowsIPv6>(),
    std::make_shared<SocketInterfaceWindowsLocal>(),
};
