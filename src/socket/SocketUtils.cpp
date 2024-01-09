#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <Logging.h>
#include "TgBotSocket.h"

static int makeSocket(bool is_client) {
    int ret = -1;
    if (!is_client) {
        LOG_D("Creating socket at " SOCKET_PATH);
    }
    struct sockaddr_un name {};
    const int sfd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (sfd < 0) {
        PLOG_E("Failed to create socket");
        return ret;
    }

    name.sun_family = AF_LOCAL;
    strncpy(name.sun_path, SOCKET_PATH, sizeof(name.sun_path));
    name.sun_path[sizeof(name.sun_path) - 1] = '\0';
    const size_t size = SUN_LEN(&name);
    decltype(&connect) fn = is_client ? connect : bind;
    if (fn(sfd, reinterpret_cast<struct sockaddr*>(&name), size) != 0) {
        PLOG_E("Failed to %s to socket", is_client ? "connect" : "bind");
        return ret;
    }
    ret = sfd;
    return ret;
}

void startListening(const listener_callback_t& cb) {
    const int sfd = makeSocket(false);
    if (sfd < 0) return;
    if (listen(sfd, 1) < 0) {
        PLOG_E("Failed to listen to socket");
        return;
    }
    LOG_I("Listening on " SOCKET_PATH);
    while (true) {
        struct sockaddr_in addr {};
        struct TgBotConnection conn {};
        unsigned int len = sizeof(addr);

        LOG_D("Waiting for connection");
        const int cfd = accept(sfd, (struct sockaddr*)&addr, &len);

        if (cfd < 0) {
            LOG_W("Accept failed, retrying");
            continue;
        } else {
            LOG_I("Client connected");
        }
        const int count = read(cfd, &conn, sizeof(conn));
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
        close(cfd);
    }
    close(sfd);
    unlink(SOCKET_PATH);
}

void writeToSocket(const struct TgBotConnection& conn) {
    const int sfd = makeSocket(true);
    if (sfd >= 0) {
        const int count = write(sfd, &conn, sizeof(conn));
        if (count < 0) {
            PLOG_E("Failed to write to socket");
        }
        close(sfd);
    }
}
