#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstddef>
#include <impl/SocketWindows.hpp>
#include <string>

#include "SocketBase.hpp"

std::optional<socket_handle_t> SocketInterfaceWindowsIPv4::createServerSocket() {
    auto context = SocketConnContext::create<sockaddr_in>();

    setOptions(Options::DESTINATION_PORT, std::to_string(kTgBotHostPort));
    if (win_helper.createInetSocketAddr(context)) {
        auto *name = static_cast<sockaddr_in *>(context.addr.get());
        auto *_name = static_cast<sockaddr *>(context.addr.get());

        name->sin_addr.s_addr = INADDR_ANY;
        if (bind(context.cfd, _name, context.addr->size) != 0) {
            WSALOG_E("Failed to bind to socket");
            closeSocketHandle(context.cfd);
            return std::nullopt;
        }
    }
    helper.inet.getExternalIP();
    return context.cfd;
}

std::optional<SocketConnContext> SocketInterfaceWindowsIPv4::createClientSocket() {
    auto context = SocketConnContext::create<sockaddr_in>();

    if (win_helper.createInetSocketAddr(context)) {
        auto *name = static_cast<sockaddr_in *>(context.addr.get());
        auto *_name = static_cast<sockaddr *>(context.addr.get());

        InetPton(AF_INET, getOptions(Options::DESTINATION_ADDRESS).c_str(),
                 &name->sin_addr);
        if (connect(context.cfd, _name, context.addr->size) != 0) {
            WSALOG_E("Failed to connect to socket");
            closeSocketHandle(context.cfd);
            return std::nullopt;
        }
    }
    return context;
}

bool SocketInterfaceWindowsIPv4::isSupported() {
    return helper.inet.isSupportedIPv4();
}

void SocketInterfaceWindowsIPv4::doGetRemoteAddr(socket_handle_t s) {
    WinHelper::doGetRemoteAddrInet<
        struct sockaddr_in, AF_INET, in_addr, INET_ADDRSTRLEN,
        offsetof(struct sockaddr_in, sin_addr)>(s);
}