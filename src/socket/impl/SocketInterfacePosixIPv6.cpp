#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <functional>

// TgBot deps
#include <Logging.h>
#include <Types.h>

#include "../SocketInterfaceUnix.h"
#include "socket/SocketInterfaceBase.h"

void SocketInterfaceUnixIPv6::foreach_ipv6_interfaces(
    const std::function<void(const char*, const char*)> callback) {
    struct ifaddrs *addrs, *tmp;
    getifaddrs(&addrs);
    tmp = addrs;
    while (tmp) {
        if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_INET6) {
            struct sockaddr_in6* pAddr = (struct sockaddr_in6*)tmp->ifa_addr;
            char ipStr[INET6_ADDRSTRLEN];

            inet_ntop(AF_INET6, &(pAddr->sin6_addr), ipStr, INET6_ADDRSTRLEN);
            callback(tmp->ifa_name, ipStr);
        }
        tmp = tmp->ifa_next;
    }
    freeifaddrs(addrs);
}

SocketInterfaceUnix::socket_handle_t
SocketInterfaceUnixIPv6::createServerSocket() {
    socket_handle_t ret = kInvalidFD;
    bool iface_done = false;
    struct sockaddr_in6 name {};
    const socket_handle_t sfd = socket(AF_INET6, SOCK_STREAM, 0);

    if (!isValidSocketHandle(sfd)) {
        PLOG_E("Failed to create socket");
        return ret;
    }

    LOG(LogLevel::DEBUG, "Dump of active interfaces' addresses (IPv6)");
    foreach_ipv6_interfaces([](const char* iface, const char* addr) {
        LOG(LogLevel::DEBUG, "ifname %s: addr %s", iface, addr);
    });
    foreach_ipv6_interfaces(
        [&iface_done, sfd](const char* iface, const char* addr) {
            if (!iface_done && strncmp("lo", iface, 2)) {
                LOG(LogLevel::DEBUG, "Choosing ifname %s addr %s", iface, addr);

                SocketHelperUnix::setSocketBindingToIface(sfd, iface);
                iface_done = true;
            }
        });

    if (!iface_done) {
        LOG(LogLevel::ERROR, "Failed to find any valid interface to bind to (IPv6)");
        return ret;
    }

    name.sin6_family = AF_INET6;
    name.sin6_port = htons(kTgBotHostPort);
    name.sin6_addr = IN6ADDR_ANY_INIT;
    if (bind(sfd, reinterpret_cast<struct sockaddr*>(&name), sizeof(name)) != 0) {
        PLOG_E("Failed to bind to socket");
        close(sfd);
        return ret;
    }
    ret = sfd;
    return ret;
}

SocketInterfaceUnix::socket_handle_t
SocketInterfaceUnixIPv6::createClientSocket() {
    socket_handle_t ret = kInvalidFD;
    struct sockaddr_in6 name {};
    const socket_handle_t sfd = socket(AF_INET6, SOCK_STREAM, 0);

    if (!isValidSocketHandle(sfd)) {
        PLOG_E("Failed to create socket");
        return ret;
    }

    name.sin6_family = AF_INET6;
    name.sin6_port = htons(kTgBotHostPort);
    inet_pton(AF_INET6, getOptions(Options::DESTINATION_ADDRESS).c_str(),
              &name.sin6_addr);
    if (connect(sfd, reinterpret_cast<struct sockaddr*>(&name), sizeof(name)) !=
        0) {
        PLOG_E("Failed to connect to socket");
        close(sfd);
        return ret;
    }
    ret = sfd;
    return ret;
}

bool SocketInterfaceUnixIPv6::isAvailable() {
    return SocketHelperCommon::isAvailableIPv6(this);
}