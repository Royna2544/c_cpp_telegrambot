#include <Logging.h>

// clang-format off
#include <winsock2.h>
#include <afunix.h>
// clang-format on

#include <chrono>
#include <string>
#include <thread>

#include "SocketUtils_internal.h"
#include "TgBotSocket.h"

#define WSALOG_E(fmt, ...) LOG_E(fmt ": %s", ##__VA_ARGS__, strWSAError(WSAGetLastError()))

static char *strWSAError(const int errcode) {
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

static SOCKET makeSocket(bool is_client) {
    struct sockaddr_un name {};
    WSADATA data;
    SOCKET fd;
    int ret;

    if (!is_client) {
        LOG_D("Creating socket at " SOCKET_PATH);
        std::remove(SOCKET_PATH);
    }

    ret = WSAStartup(MAKEWORD(2, 2), &data);
    if (ret != 0) {
        LOG_E("WSAStartup failed");
        return INVALID_SOCKET;
    }
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == INVALID_SOCKET) {
        WSALOG_E("Failed to create socket");
        WSACleanup();
        return INVALID_SOCKET;
    }

    name.sun_family = AF_UNIX;
    strncpy(name.sun_path, SOCKET_PATH, sizeof(name.sun_path));
    name.sun_path[sizeof(name.sun_path) - 1] = '\0';

    decltype(&connect) fn = is_client ? connect : bind;
    if (fn(fd, reinterpret_cast<struct sockaddr *>(&name), sizeof(name)) != 0) {
        WSALOG_E("Failed to %s to socket", is_client ? "connect" : "bind");
        closesocket(fd);
        WSACleanup();
        return INVALID_SOCKET;
    }
    return fd;
}

void startListening(const listener_callback_t &cb, std::promise<bool> &createdPromise) {
    bool should_break = false;
    const SOCKET sfd = makeSocket(false);
    if (sfd != INVALID_SOCKET) {
        do {
            if (listen(sfd, SOMAXCONN) == SOCKET_ERROR) {
                WSALOG_E("Failed to listen to socket");
                break;
            }
            LOG_I("Listening on " SOCKET_PATH);
            createdPromise.set_value(true);
            while (!should_break) {
                struct sockaddr_un addr {};
                struct TgBotConnection conn {};
                int len = sizeof(addr);

                LOG_D("Waiting for connection");
                const SOCKET cfd = accept(sfd, (struct sockaddr *)&addr, &len);

                if (cfd == INVALID_SOCKET) {
                    WSALOG_E("Accept failed.");
                    break;
                } else {
                    LOG_D("Client connected");
                }
                const int count = recv(cfd, reinterpret_cast<char *>(&conn), sizeof(conn), 0);
                should_break = handleIncomingBuf(count, conn, cb, [] { return strWSAError(WSAGetLastError()); });
                closesocket(cfd);
            }
        } while (false);
        closesocket(sfd);
        WSACleanup();
        return;
    }
    createdPromise.set_value(false);
}

void writeToSocket(struct TgBotConnection conn) {
    const auto sfd = makeSocket(true);
    if (sfd != INVALID_SOCKET) {
        const int count = send(sfd, reinterpret_cast<char *>(&conn), sizeof(conn), 0);
        if (count < 0) {
            WSALOG_E("Failed to send to socket");
        }
        shutdown(sfd, SD_BOTH);
        closesocket(sfd);
        WSACleanup();
    }
}

void forceStopListening(void) {
    // TODO: Unimplemented
}
