#include <cstddef>
#include <impl/SocketWindows.hpp>
#include <string>

#include "../helper/HelperWindows.hpp"

std::optional<socket_handle_t>
SocketInterfaceWindowsIPv4::createServerSocket() {
    auto context = SocketConnContext::create<sockaddr_in>();

    if (win_helper.createInetSocketAddr(context)) {
        auto *name = static_cast<sockaddr_in *>(context.addr.get());
        auto *_name = static_cast<sockaddr *>(context.addr.get());

        name->sin_addr.s_addr = INADDR_ANY;
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
SocketInterfaceWindowsIPv4::createClientSocket() {
    auto context = SocketConnContext::create<sockaddr_in>();

    if (win_helper.createInetSocketAddr(context)) {
        auto *name = static_cast<sockaddr_in *>(context.addr.get());
        auto *_name = static_cast<sockaddr *>(context.addr.get());

        InetPton(AF_INET, options.address.get().c_str(), &name->sin_addr);
        if (connect(context.cfd, _name, context.addr->size) != 0) {
            LOG(ERROR) << "Failed to connect to socket: " << WSALastErrorStr();
            closeSocketHandle(context.cfd);
            return std::nullopt;
        }
    }
    return context;
}

void SocketInterfaceWindowsIPv4::doGetRemoteAddr(socket_handle_t s) {
    printRemoteAddress<sockaddr_in, in_addr, AF_INET>(s);
}