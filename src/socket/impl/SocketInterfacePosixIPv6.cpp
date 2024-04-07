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
#include <Types.h>
#include <absl/log/log.h>

#include "../SocketInterfaceUnix.h"
#include "socket/SocketInterfaceBase.h"

void SocketInterfaceUnixIPv6::foreach_ipv6_interfaces(
    const std::function<void(const char*, const char*)> callback) {
    struct ifaddrs *addrs = nullptr, *tmp = nullptr;
    getifaddrs(&addrs);
    tmp = addrs;
    while (tmp) {
        if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_INET6) {
            struct sockaddr_in6* pAddr = reinterpret_cast<struct sockaddr_in6*>(tmp->ifa_addr);
            std::array<char, INET6_ADDRSTRLEN> ipStr;

            inet_ntop(AF_INET6, &(pAddr->sin6_addr), ipStr.data(), INET6_ADDRSTRLEN);
            callback(tmp->ifa_name, ipStr.data());
        }
        tmp = tmp->ifa_next;
    }
    freeifaddrs(addrs);
}

socket_handle_t SocketInterfaceUnixIPv6::createServerSocket() {
    socket_handle_t ret = kInvalidFD;
    bool iface_done = false;
    struct sockaddr_in6 name {};
    struct sockaddr* _name = reinterpret_cast<struct sockaddr*>(&name);
    const socket_handle_t sfd = socket(AF_INET6, SOCK_STREAM, 0);

    if (!isValidSocketHandle(sfd)) {
        PLOG(ERROR) << "Failed to create socket";
        return ret;
    }

    LOG(INFO) << "Dump of active interfaces' addresses (IPv6)";
    foreach_ipv6_interfaces([](const char* iface, const char* addr) {
        LOG(INFO) << "ifname " << iface << ": addr " << addr;
    });
    foreach_ipv6_interfaces(
        [&iface_done, sfd](const char* iface, const char* addr) {
            if (!iface_done && strncmp("lo", iface, 2) != 0) {
                LOG(INFO) << "Choosing ifname " << iface;

                SocketHelperUnix::setSocketBindingToIface(sfd, iface);
                iface_done = true;
            }
        });

    if (!iface_done) {
        LOG(ERROR) << "Failed to find any valid interface to bind to (IPv6)";
        return ret;
    }
    SocketHelperCommon::printExternalIPINet();

    name.sin6_family = AF_INET6;
    name.sin6_port = htons(kTgBotHostPort);
    name.sin6_addr = IN6ADDR_ANY_INIT;
    if (bind(sfd, _name, sizeof(name)) != 0) {
        PLOG(ERROR) << "Failed to bind to socket";
        close(sfd);
        return ret;
    }
    ret = sfd;
    return ret;
}

socket_handle_t SocketInterfaceUnixIPv6::createClientSocket() {
    socket_handle_t ret = kInvalidFD;
    struct sockaddr_in6 name {};
    struct sockaddr* _name = reinterpret_cast<struct sockaddr*>(&name);
    const socket_handle_t sfd = socket(AF_INET6, SOCK_STREAM, 0);

    if (!isValidSocketHandle(sfd)) {
        PLOG(ERROR) << "Failed to create socket";
        return ret;
    }

    name.sin6_family = AF_INET6;
    name.sin6_port = htons(SocketHelperCommon::getPortNumInet(this));
    inet_pton(AF_INET6, getOptions(Options::DESTINATION_ADDRESS).c_str(),
              &name.sin6_addr);
    if (connect(sfd, _name, sizeof(name)) != 0) {
        PLOG(ERROR) << "Failed to connect to socket";
        close(sfd);
        return ret;
    }
    ret = sfd;
    return ret;
}

bool SocketInterfaceUnixIPv6::isAvailable() {
    return SocketHelperCommon::isAvailableIPv6(this);
}

void SocketInterfaceUnixIPv6::stopListening(const std::string& e) {
    forceStopListening();
}

void SocketInterfaceUnixIPv6::doGetRemoteAddr(socket_handle_t s) {
    SocketHelperUnix::doGetRemoteAddrInet<
        struct sockaddr_in6, AF_INET6, in6_addr, INET6_ADDRSTRLEN,
        offsetof(struct sockaddr_in6, sin6_addr)>(s);
}