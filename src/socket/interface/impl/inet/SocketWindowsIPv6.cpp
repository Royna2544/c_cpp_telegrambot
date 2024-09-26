#include <impl/SocketWindows.hpp>

#include "../helper/HelperWindows.hpp"

std::optional<socket_handle_t>
SocketInterfaceWindowsIPv6::createServerSocket() {
    auto context = SocketConnContext::create<sockaddr_in6>();

    if (win_helper.createInet6SocketAddr(context)) {
        auto *name = static_cast<sockaddr_in6 *>(context.addr.get());
        auto *_name = static_cast<sockaddr *>(context.addr.get());

        name->sin6_addr = in6addr_any;
        if (bind(context.cfd, _name, context.addr->size()) != 0) {
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

        InetPton(AF_INET6, options.address.get().c_str(), &name->sin_addr);
        if (connect(context.cfd, _name, context.addr->size()) != 0) {
            LOG(ERROR) << "Failed to connect to socket: " << WSALastErrorStr();
            closeSocketHandle(context.cfd);
            return std::nullopt;
        }
    }
    return context;
}

void SocketInterfaceWindowsIPv6::printRemoteAddress(socket_handle_t s) {
    printRemoteAddress_impl<sockaddr_in6, in6_addr, AF_INET6>(s);
}