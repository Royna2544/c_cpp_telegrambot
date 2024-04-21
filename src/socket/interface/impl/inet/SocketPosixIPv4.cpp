#include <absl/log/log.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <functional>
#include <impl/SocketPosix.hpp>

socket_handle_t SocketInterfaceUnixIPv4::createServerSocket() {
    socket_handle_t ret = kInvalidFD;
    bool iface_done = false;
    struct sockaddr_in name {};
    auto* _name = reinterpret_cast<struct sockaddr*>(&name);
    const socket_handle_t sfd = socket(AF_INET, SOCK_STREAM, 0);

    if (!isValidSocketHandle(sfd)) {
        PLOG(ERROR) << "Failed to create socket";
        return ret;
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
        close(sfd);
        return ret;
    }
    ret = sfd;
    return ret;
}

socket_handle_t SocketInterfaceUnixIPv4::createClientSocket() {
    socket_handle_t ret = kInvalidFD;
    struct sockaddr_in name {};
    auto* _name = reinterpret_cast<struct sockaddr*>(&name);
    const socket_handle_t sfd = socket(AF_INET, SOCK_STREAM, 0);

    if (!isValidSocketHandle(sfd)) {
        PLOG(ERROR) << "Failed to create socket";
        return ret;
    }

    name.sin_family = AF_INET;
    name.sin_port = htons(helper.inet.getPortNum());
    inet_aton(getOptions(Options::DESTINATION_ADDRESS).c_str(), &name.sin_addr);
    if (connect(sfd, _name, sizeof(name)) != 0) {
        PLOG(ERROR) << "Failed to connect to socket";
        close(sfd);
        return ret;
    }
    ret = sfd;
    return ret;
}

bool SocketInterfaceUnixIPv4::isAvailable() {
    return helper.inet.isAvailableIPv4();
}

void SocketInterfaceUnixIPv4::doGetRemoteAddr(socket_handle_t s) {
    SocketHelperUnix::getremoteaddr_ipv4::run(s);
}