#include <CStringLifetime.h>
#include <Types.h>
#include <absl/log/log.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <filesystem>
#include <libos/libfs.hpp>

#include "../SocketInterfaceUnix.h"
#include "socket/SocketInterfaceBase.h"
#include "socket/TgBotSocket.h"

socket_handle_t SocketInterfaceUnixLocal::makeSocket(bool client) {
    int ret = kInvalidFD;
    struct sockaddr_un name {};
    struct sockaddr* _name = reinterpret_cast<struct sockaddr*>(&name);
    CStringLifetime path = getOptions(Options::DESTINATION_ADDRESS);

    if (!client) {
        LOG(INFO) << "Creating socket at " << path.get();
    }
    const int sfd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (sfd < 0) {
        PLOG(ERROR) << "Failed to create socket";
        return ret;
    }

    name.sun_family = AF_LOCAL;
    strncpy(name.sun_path, path.get(), sizeof(name.sun_path));
    name.sun_path[sizeof(name.sun_path) - 1] = '\0';
    const size_t size = SUN_LEN(&name);
    decltype(&connect) fn = client ? connect : bind;
    if (fn(sfd, _name, size) != 0) {
        do {
            if (client) {
                PLOG(ERROR) << "Failed to connect to socket";
            } else {
                PLOG(ERROR) << "Failed to bind to socket";
            }
            if (!client && errno == EADDRINUSE) {
                cleanupServerSocket();
                if (fn(sfd, _name, size) == 0) {
                    LOG(INFO) << "Bind succeeded by removing socket file";
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

socket_handle_t SocketInterfaceUnixLocal::createClientSocket() {
    setOptions(Options::DESTINATION_ADDRESS, getSocketPath().string());
    return makeSocket(/*client=*/true);
}

socket_handle_t SocketInterfaceUnixLocal::createServerSocket() {
    setOptions(Options::DESTINATION_ADDRESS, getSocketPath().string());
    return makeSocket(/*client=*/false);
}
void SocketInterfaceUnixLocal::cleanupServerSocket() {
    SocketHelperCommon::cleanupServerSocketLocalSocket(this);
}

bool SocketInterfaceUnixLocal::canSocketBeClosed() {
    return SocketHelperCommon::canSocketBeClosedLocalSocket(this);
}

bool SocketInterfaceUnixLocal::isAvailable() {
    return SocketHelperCommon::isAvailableLocalSocket();
}

void SocketInterfaceUnixLocal::doGetRemoteAddr(socket_handle_t s) {}