#include <absl/log/log.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <impl/SocketPosix.hpp>

#include "HelperPosix.hpp"

std::optional<socket_handle_t> SocketInterfaceUnixIPv4::createServerSocket() {
    socket_handle_t ret = kInvalidFD;
    bool iface_done = false;
    struct sockaddr_in name {};
    auto* _name = reinterpret_cast<struct sockaddr*>(&name);
    socket_handle_t sfd = socket(AF_INET, SOCK_STREAM, 0);

    if (!isValidSocketHandle(sfd)) {
        PLOG(ERROR) << "Failed to create socket";
        return std::nullopt;
    }

    LOG(INFO) << "[IPv4] Dump of active interfaces' addresses";
    forEachINetAddress<sockaddr_in, in_addr, AF_INET>([](const auto& data) {
        LOG(INFO) << "[IPv4] ifname " << data.name << ": addr " << data.addr;
    });
    forEachINetAddress<sockaddr_in, in_addr, AF_INET>(
        [&iface_done, sfd](const auto& data) {
            if (!iface_done && kLocalInterface.data() != data.name) {
                LOG(INFO) << "[IPv4] Choosing ifname " << data.name;
                bindToInterface(sfd, data.name);
                iface_done = true;
            }
        });

    if (!iface_done) {
        LOG(ERROR) << "[IPv4] Failed to find any valid interface to bind to";
        return ret;
    }
    helper.inet.getExternalIP();

    name.sin_family = AF_INET;
    name.sin_port = htons(helper.inet.getPortNum());
    name.sin_addr.s_addr = INADDR_ANY;
    if (bind(sfd, _name, sizeof(name)) != 0) {
        PLOG(ERROR) << "Failed to bind to socket";
        closeSocketHandle(sfd);
        return std::nullopt;
    }
    ret = sfd;
    return ret;
}

std::optional<SocketConnContext> SocketInterfaceUnixIPv4::createClientSocket() {
    SocketConnContext ctx = SocketConnContext::create<sockaddr_in>();
    struct sockaddr_in name {};
    const auto* _name = reinterpret_cast<struct sockaddr*>(&name);

    ctx.cfd = socket(AF_INET, SOCK_STREAM, 0);
    if (!isValidSocketHandle(ctx.cfd)) {
        PLOG(ERROR) << "Failed to create socket";
        return std::nullopt;
    }

    name.sin_family = AF_INET;
    name.sin_port = htons(helper.inet.getPortNum());
    inet_pton(AF_INET, getOptions(Options::DESTINATION_ADDRESS).c_str(),
              &name.sin_addr);
    if (connect(ctx.cfd, _name, sizeof(name)) != 0) {
        PLOG(ERROR) << "Failed to connect to socket";
        closeSocketHandle(ctx.cfd);
        return std::nullopt;
    }
    ctx.addr.assignFrom(name);
    return ctx;
}

void SocketInterfaceUnixIPv4::doGetRemoteAddr(socket_handle_t s) {
    printRemoteAddress<sockaddr_in, in_addr, AF_INET>(s);
}