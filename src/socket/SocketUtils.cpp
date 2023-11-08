#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "../utils/libutils.h"
#include "TgBotSocket.h"

static int makeSocket(bool is_client) {
    int ret = -1;
    if (!is_client && access(SOCKET_PATH, F_OK) == 0) {
        if (unlink(SOCKET_PATH) != 0) {
            LOG_E("Failed to remove existing socket file");
            return ret;
        }
        LOG_D("Creating socket at " SOCKET_PATH);
    }
    struct sockaddr_un name;
    int sfd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (sfd < 0) {
        LOG_E("Failed to create socket");
        return ret;
    }

    name.sun_family = AF_LOCAL;
    strncpy(name.sun_path, SOCKET_PATH, sizeof(name.sun_path));
    name.sun_path[sizeof(name.sun_path) - 1] = '\0';
    size_t size = SUN_LEN(&name);
    decltype(&connect) fn;
    if (is_client) {
        fn = connect;
    } else {
        fn = bind;
    }
    if (fn(sfd, (struct sockaddr *)&name, size) != 0) {
        LOG_E("Failed to %s to socket", is_client ? "connect" : "bind");
        return ret;
    }
    ret = sfd;
    return ret;
}

void startListening(listener_callback_t cb) {
    int sfd = makeSocket(false);
    if (sfd < 0) return;
    if (listen(sfd, 1) < 0) {
        LOG_E("Failed to listen to socket");
        return;
    }
    LOG_I("Listening on " SOCKET_PATH);
    while (1) {
        struct sockaddr_in addr;
        unsigned int len = sizeof(addr);

        LOG_D("Waiting for connection");
        int cfd = accept(sfd, (struct sockaddr *)&addr, &len);

        if (cfd < 0) {
            LOG_W("Accept failed, retrying");
            continue;
        } else {
            LOG_I("Client connected");
	}
        struct TgBotConnection conn;
        int count = read(cfd, &conn, sizeof(conn));
        if (count > 0) {
            if (conn.cmd == CMD_EXIT) {
                LOG_D("Received exit command");
                break;
            }
            LOG_D("Received buf with cmd %d, invoke callback!", conn.cmd);
            cb(conn);
        } else {
            LOG_E("read failed, %s", strerror(errno));
        }
	close(cfd);
    }
    close(sfd);
    unlink(SOCKET_PATH);
}

void writeToSocket(struct TgBotConnection conn) {
    int sfd = makeSocket(true);
    if (sfd < 0) return;
    int count = write(sfd, &conn, sizeof(conn));
    if (count < 0) {
        LOG_W("While writing to socket, %s", strerror(errno));
    }
    close(sfd);
}
