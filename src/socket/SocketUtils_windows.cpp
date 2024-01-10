#include <winsock2.h>
#include <afunix.h>

#include <thread>
#include <chrono>

#include <Logging.h>
#include "TgBotSocket.h"

#define WSALOG_E(fmt, ...) LOG_E(fmt ": ret %d", ##__VA_ARGS__, WSAGetLastError())
#define WSALOG_W(fmt, ...) LOG_W(fmt ": ret %d", ##__VA_ARGS__, WSAGetLastError())

static SOCKET makeSocket(bool is_client) {
    struct sockaddr_un name {};
    WSADATA data;
    SOCKET fd;
    int ret;

    if (!is_client) {
        LOG_D("Creating socket at " SOCKET_PATH);
        std::remove(SOCKET_PATH);
    }

    ret = WSAStartup(MAKEWORD(2,2), &data);
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
    if (fn(fd, reinterpret_cast<struct sockaddr*>(&name), sizeof(name)) != 0) {
        WSALOG_E("Failed to %s to socket", is_client ? "connect" : "bind");
        closesocket(fd);
        WSACleanup();
        return INVALID_SOCKET;
    }
    return fd;
}

void startListening(const listener_callback_t& cb) {
    const SOCKET sfd = makeSocket(false);
    if (sfd != INVALID_SOCKET) {
        if (listen(sfd, SOMAXCONN) == SOCKET_ERROR) {
            WSALOG_E("Failed to listen to socket");
            return;
        }
        LOG_I("Listening on " SOCKET_PATH);
        while (true) {
            struct sockaddr_un addr {};
            struct TgBotConnection conn {};
            int len = sizeof(addr);

            LOG_D("Waiting for connection");
            const SOCKET cfd = accept(sfd, (struct sockaddr*)&addr, &len);

            if (cfd == INVALID_SOCKET) {
                WSALOG_W("Accept failed. retry");
                std::this_thread::sleep_for(std::chrono::seconds(4));
                continue;
            } else {
                LOG_I("Client connected");
            }
            const int count = recv(cfd, reinterpret_cast<char *>(&conn), sizeof(conn), 0);
            if (count > 0) {
                if (conn.cmd == CMD_EXIT) {
                    LOG_D("Received exit command");
                    break;
                }
                LOG_D("Received buf with %s, invoke callback!", toStr(conn.cmd).c_str());
                cb(conn);
            } else {
                PLOG_E("Failed to read from socket");
            }
            closesocket(cfd);
        }
        closesocket(sfd);
    }
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
