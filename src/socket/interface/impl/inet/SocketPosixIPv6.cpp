#include <absl/log/log.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <functional>
#include <impl/SocketPosix.hpp>

#include "HelperPosix.hpp"

std::optional<socket_handle_t> SocketInterfaceUnixIPv6::createServerSocket() {
    socket_handle_t ret = kInvalidFD;
    bool iface_done = false;
    struct sockaddr_in6 name {};
    auto* _name = reinterpret_cast<struct sockaddr*>(&name);
    socket_handle_t sfd = socket(AF_INET6, SOCK_STREAM, 0);

    if (!isValidSocketHandle(sfd)) {
        PLOG(ERROR) << "Failed to create socket";
        return std::nullopt;
    }

    LOG(INFO) << "[IPv6] Dump of active interfaces' addresses";
    forEachINetAddress<sockaddr_in6, in6_addr, AF_INET6>([](const auto& data) {
        LOG(INFO) << "[IPv6] ifname " << data.name << ": addr " << data.addr;
    });
    forEachINetAddress<sockaddr_in6, in6_addr, AF_INET6>(
        [&iface_done, sfd](const auto& data) {
            if (!iface_done && kLocalInterface.data() != data.name) {
                LOG(INFO) << "[IPv6] Choosing ifname " << data.name;
                bindToInterface(sfd, data.name);
                iface_done = true;
            }
        });
    if (!iface_done) {
        LOG(ERROR) << "[IPv6] Failed to find any valid interface to bind to";
        return std::nullopt;
    }
    helper.inet.getExternalIP();

    name.sin6_family = AF_INET6;
    name.sin6_port = htons(helper.inet.getPortNum());
    name.sin6_addr = IN6ADDR_ANY_INIT;
    if (bind(sfd, _name, sizeof(name)) != 0) {
        PLOG(ERROR) << "Failed to bind to socket";
        closeSocketHandle(sfd);
        return std::nullopt;
    }
    ret = sfd;
    return ret;
}

std::optional<SocketConnContext> SocketInterfaceUnixIPv6::createClientSocket() {
    SocketConnContext ctx = SocketConnContext::create<sockaddr_in6>();
    struct sockaddr_in6 name {};
    auto* _name = reinterpret_cast<struct sockaddr*>(&name);

    ctx.cfd = socket(AF_INET6, SOCK_STREAM, 0);
    if (!isValidSocketHandle(ctx.cfd)) {
        PLOG(ERROR) << "Failed to create socket";
        return std::nullopt;
    }

    name.sin6_family = AF_INET6;
    name.sin6_port = htons(helper.inet.getPortNum());
    inet_pton(AF_INET6, getOptions(Options::DESTINATION_ADDRESS).c_str(),
              &name.sin6_addr);
    if (connect(ctx.cfd, _name, sizeof(name)) != 0) {
        PLOG(ERROR) << "Failed to connect to socket";
        closeSocketHandle(ctx.cfd);
        return std::nullopt;
    }
    ctx.addr.assignFrom(name);
    return ctx;
}

void SocketInterfaceUnixIPv6::doGetRemoteAddr(socket_handle_t s) {
    printRemoteAddress<sockaddr_in6, in6_addr, AF_INET6>(s);
}