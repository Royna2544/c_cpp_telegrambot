#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstddef>
#include <impl/SocketWindows.hpp>
#include <string>

socket_handle_t SocketInterfaceWindowsIPv4::createServerSocket() {
    struct sockaddr_in name {};
    socket_handle_t sfd;

    setOptions(Options::DESTINATION_PORT, std::to_string(kTgBotHostPort));
    if (SocketHelperWindows::createInetSocketAddr(&sfd, &name, this)) {
        name.sin_addr.s_addr = INADDR_ANY;
        if (bind(sfd, reinterpret_cast<struct sockaddr *>(&name),
                 sizeof(name)) != 0) {
            WSALOG_E("Failed to bind to socket");
            closesocket(sfd);
            return INVALID_SOCKET;
        }
    }
    helper.inet.printExternalIP();
    return sfd;
}

socket_handle_t SocketInterfaceWindowsIPv4::createClientSocket() {
    struct sockaddr_in name {};
    socket_handle_t sfd;

    if (SocketHelperWindows::createInetSocketAddr(&sfd, &name, this)) {
        InetPton(AF_INET, getOptions(Options::DESTINATION_ADDRESS).c_str(),
                 &name.sin_addr);
        if (connect(sfd, reinterpret_cast<struct sockaddr *>(&name),
                    sizeof(name)) != 0) {
            WSALOG_E("Failed to connect to socket");
            closesocket(sfd);
            return INVALID_SOCKET;
        }
    }
    return sfd;
}

bool SocketInterfaceWindowsIPv4::isSupported() {
    return helper.inet.isSupportedIPv4();
}

void SocketInterfaceWindowsIPv4::doGetRemoteAddr(socket_handle_t s) {
    SocketHelperWindows::doGetRemoteAddrInet<
        struct sockaddr_in, AF_INET, in_addr, INET_ADDRSTRLEN,
        offsetof(struct sockaddr_in, sin_addr)>(s);
}