#include <Logging.h>
#include <Types.h>
#include <arpa/inet.h>
#include <libos/libfs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <filesystem>

#include "../SocketInterfaceUnix.h"

SocketInterfaceUnix::socket_handle_t SocketInterfaceUnixLocal::makeSocket(
    bool client) {
    int ret = kInvalidFD;
    if (!client) {
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
    decltype(&connect) fn = client ? connect : bind;
    if (fn(sfd, reinterpret_cast<struct sockaddr*>(&name), size) != 0) {
        do {
            PLOG_E("Failed to %s to socket", client ? "connect" : "bind");
            if (!client && errno == EADDRINUSE) {
                std::remove(SOCKET_PATH);
                if (fn(sfd, reinterpret_cast<struct sockaddr*>(&name), size) ==
                    0) {
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

SocketInterfaceUnix::socket_handle_t
SocketInterfaceUnixLocal::createClientSocket() {
    return makeSocket(/*client=*/true);
}

SocketInterfaceUnix::socket_handle_t
SocketInterfaceUnixLocal::createServerSocket() {
    return makeSocket(/*client=*/false);
}
void SocketInterfaceUnixLocal::cleanupServerSocket() {
    std::filesystem::remove(SOCKET_PATH);
}

bool SocketInterfaceUnixLocal::canSocketBeClosed() {
    bool socketValid = true;

    if (!fileExists(SOCKET_PATH)) {
        LOG_W("Socket file was deleted");
        socketValid = false;
    }
    return socketValid;
}