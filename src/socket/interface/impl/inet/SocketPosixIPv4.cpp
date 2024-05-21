#include <absl/log/log.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <functional>
#include <impl/SocketPosix.hpp>

#include "SharedMalloc.hpp"
#include "SocketBase.hpp"

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

    LOG(INFO) << "Dump of active interfaces' addresses (IPv4)";
    SocketHelperUnix::foreach_ipv4_interfaces::run(
        [](const char* iface, const char* addr) {
            LOG(INFO) << "ifname " << iface << ": addr " << addr;
        });
    SocketHelperUnix::foreach_ipv4_interfaces::run(
        [&iface_done, sfd](const char* iface, const char* /*addr*/) {
            if (!iface_done && strncmp("lo", iface, 2) != 0) {
                LOG(INFO) << "Choosing ifname " << iface;

                SocketHelperUnix::setSocketBindingToIface(sfd, iface);
                iface_done = true;
            }
        });

    if (!iface_done) {
        LOG(ERROR) << "Failed to find any valid interface to bind to (IPv4)";
        return ret;
    }
    helper.inet.printExternalIP();

    name.sin_family = AF_INET;
    name.sin_port = htons(kTgBotHostPort);
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

bool SocketInterfaceUnixIPv4::isSupported() {
    return helper.inet.isSupportedIPv4();
}

void SocketInterfaceUnixIPv4::doGetRemoteAddr(socket_handle_t s) {
    SocketHelperUnix::getremoteaddr_ipv4::run(s);
}