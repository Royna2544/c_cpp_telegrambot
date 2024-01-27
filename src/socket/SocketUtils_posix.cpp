#include <Logging.h>
#include <poll.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <thread>

#include "SocketUtils_internal.h"
#include "TgBotSocket.h"

static pipe_t kListenTerminate;
int& listen_fd = kListenTerminate[0];
int& notify_fd = kListenTerminate[1];

using kListenData = char;

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
        do {
            PLOG_E("Failed to %s to socket", is_client ? "connect" : "bind");
            if (!is_client && errno == EADDRINUSE) {
                std::remove(SOCKET_PATH);
                if (fn(sfd, reinterpret_cast<struct sockaddr*>(&name), size) == 0) {
                    LOG_I("Bind succeeded by removing socket file");
                    break;
                }
            }
            close(sfd);
            return ret;
        } while (false);
    }
    ret = sfd;
    return ret;
}

bool startListening(const listener_callback_t& cb) {
    bool should_break = false;
    int rc = 0;
    const int sfd = makeSocket(false);

    if (sfd >= 0) {
        if (listen(sfd, 1) < 0) {
            PLOG_E("Failed to listen to socket");
            return false;
        }
        LOG_I("Listening on " SOCKET_PATH);
        rc = pipe(kListenTerminate);
        if (rc < 0) {
            PLOG_E("Pipe failed");
            return false;
        }
        while (!should_break) {
            struct sockaddr_un addr {};
            struct TgBotConnection conn {};
            socklen_t len = sizeof(addr);
            struct pollfd fds[] = {
                {
                    .fd = listen_fd,
                    .events = POLLIN,
                    .revents = 0,
                },
                {
                    .fd = sfd,
                    .events = POLLIN,
                    .revents = 0,

                }};
            const pollfd& listen_fd_poll = fds[0];
            const pollfd& socket_fd_poll = fds[1];

            LOG_D("Waiting for incoming events");

            rc = poll(fds, sizeof(fds) / sizeof(pollfd), -1);
            if (rc < 0) {
                PLOG_E("Poll failed");
                break;
            }

            if (rc == 2) {
                LOG_W("Dropping incoming buffer: exiting");
            }
            if (listen_fd_poll.revents & POLLIN) {
                kListenData buf;
                read(listen_fd, &buf, sizeof(kListenData));
                close(listen_fd);
                listen_fd = -1;
            } else if (!(socket_fd_poll.revents & POLLIN)) {
                LOG_E("Unexpected state: sfd.revents: %d, listen_fd.revents: %d",
                      socket_fd_poll.revents, listen_fd_poll.revents);
                break;
            }

            const int cfd = accept(sfd, (struct sockaddr*)&addr, &len);

            if (cfd < 0) {
                PLOG_E("Accept failed");
                break;
            } else {
                LOG_D("Client connected");
            }
            const int count = read(cfd, &conn, sizeof(conn));
            should_break = handleIncomingBuf(count, conn, cb, strerror(errno));
            close(cfd);
        }
        close(sfd);
        unlink(SOCKET_PATH);
        return true;
    }
    return false;
}

void writeToSocket(struct TgBotConnection conn) {
    const int sfd = makeSocket(true);
    if (sfd >= 0) {
        const int count = write(sfd, &conn, sizeof(conn));
        if (count < 0) {
            PLOG_E("Failed to write to socket");
        }
        close(sfd);
    }
}

void forceStopListening(void) {
    if (notify_fd > 0 && listen_fd > 0) {
        kListenData d = 0;
        write(notify_fd, &d, sizeof(kListenData));
        close(notify_fd);
        notify_fd = -1;
    }
}
