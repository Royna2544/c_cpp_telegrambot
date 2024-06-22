#include <impl/SocketWindows.hpp>

#include "SocketBase.hpp"


std::optional<socket_handle_t>
SocketInterfaceWindowsIPv6::createServerSocket() {
    auto context = SocketConnContext::create<sockaddr_in6>();

    setOptions(Options::DESTINATION_PORT, std::to_string(kTgBotHostPort));
    if (win_helper.createInet6SocketAddr(context)) {
        auto *name = static_cast<sockaddr_in6 *>(context.addr.get());
        auto *_name = static_cast<sockaddr *>(context.addr.get());

        name->sin6_addr = in6addr_any;
        if (bind(context.cfd, _name, context.addr->size) != 0) {
            LOG(ERROR) << "Failed to bind to socket: " << WSALastErrorStr();
            closeSocketHandle(context.cfd);
            return std::nullopt;
        }
    }
    helper.inet.getExternalIP();
    return context.cfd;
}

std::optional<SocketConnContext>
SocketInterfaceWindowsIPv6::createClientSocket() {
    auto context = SocketConnContext::create<sockaddr_in>();

    if (win_helper.createInet6SocketAddr(context)) {
        auto *name = static_cast<sockaddr_in *>(context.addr.get());
        auto *_name = static_cast<sockaddr *>(context.addr.get());

        InetPton(AF_INET, getOptions(Options::DESTINATION_ADDRESS).c_str(),
                 &name->sin_addr);
        if (connect(context.cfd, _name, context.addr->size) != 0) {
            LOG(ERROR) << "Failed to connect to socket: " << WSALastErrorStr();
            closeSocketHandle(context.cfd);
            return std::nullopt;
        }
    }
    return context;
}

void SocketInterfaceWindowsIPv6::doGetRemoteAddr(socket_handle_t s) {
    WinHelper::doGetRemoteAddrInet<struct sockaddr_in6, AF_INET6, in6_addr,
                                   INET6_ADDRSTRLEN,
                                   offsetof(struct sockaddr_in6, sin6_addr)>(s);
}