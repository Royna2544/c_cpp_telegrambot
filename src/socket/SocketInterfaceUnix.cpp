#include "SocketInterfaceUnix.h"

#include <absl/log/log.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

void SocketInterfaceUnix::startListening(const listener_callback_t& listen_cb,
                                         const result_callback_t& result_cb) {
    bool should_break = false, value_set = false;
    int rc = 0;
    socket_handle_t sfd = createServerSocket();
    if (isValidSocketHandle(sfd)) {
        do {
            if (listen(sfd, 1) < 0) {
                PLOG(ERROR) << "Failed to listen to socket";
                break;
            }
            rc = pipe(kListenTerminate);
            if (rc < 0) {
                PLOG(ERROR) << "Pipe failed";
                break;
            }
            result_cb(true);
            value_set = true;
            while (!should_break) {
                struct sockaddr_un addr {};
                struct TgBotConnection conn {};
                socklen_t len = sizeof(addr);
                std::array<struct pollfd, 2> fds = {};
                const struct pollfd listen_fd_poll = {
                    .fd = listen_fd,
                    .events = POLLIN,
                    .revents = 0,
                };
                const struct pollfd socket_fd_poll = {
                    .fd = sfd,
                    .events = POLLIN,
                    .revents = 0,
                };
                fds = {listen_fd_poll, socket_fd_poll};

                DLOG(INFO) << "Waiting for incoming events";

                rc = poll(fds.data(), fds.size(), -1);
                if (rc < 0) {
                    PLOG(ERROR) << "Poll failed";
                    break;
                }

                if (rc == 2) {
                    LOG(WARNING) << "Dropping incoming buffer: exiting";
                }
                if (listen_fd_poll.revents & POLLIN) {
                    dummy_listen_buf_t buf = {};
                    ssize_t rc =
                        read(listen_fd, &buf, sizeof(dummy_listen_buf_t));
                    if (rc < 0) PLOG(ERROR) << "Reading data from forcestop fd";
                    closeFd(listen_fd);
                    break;
                } else if (!(socket_fd_poll.revents & POLLIN)) {
                    LOG(ERROR)
                        << "Unexpected state: sfd.revents: "
                        << socket_fd_poll.revents
                        << ", listen_fd.revents: " << listen_fd_poll.revents;
                    break;
                }

                const socket_handle_t cfd = accept(
                    sfd, reinterpret_cast<struct sockaddr*>(&addr), &len);

                if (!isValidSocketHandle(cfd)) {
                    PLOG(ERROR) << "Accept failed";
                    break;
                } else {
                    doGetRemoteAddr(cfd);
                }
                const ssize_t count = read(cfd, &conn, sizeof(conn));
                should_break = handleIncomingBuf(
                    count, conn, listen_cb, [] { return strerror(errno); });
                close(cfd);
            }
        } while (false);
        closePipe(kListenTerminate);
        close(sfd);
        cleanupServerSocket();
    }
    if (!value_set) result_cb(false);
}

void SocketInterfaceUnix::writeToSocket(struct TgBotConnection conn) {
    const socket_handle_t sfd = createClientSocket();
    if (isValidSocketHandle(sfd)) {
        const ssize_t count = write(sfd, &conn, sizeof(conn));
        if (count < 0) {
            PLOG(ERROR) << "Failed to write to socket";
        }
        close(sfd);
    }
}

void SocketInterfaceUnix::forceStopListening(void) {
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