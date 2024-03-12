#include <winsock2.h>
#include <ws2tcpip.h>

#include "../SocketInterfaceWindows.h"
#include "socket/SocketInterfaceBase.h"

SocketInterfaceWindows::socket_handle_t
SocketInterfaceWindowsIPv6::createServerSocket() {
    struct sockaddr_in6 name {};
    socket_handle_t sfd;
    WSADATA data;

    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        WSALOG_E("WSAStartup failed");
        return INVALID_SOCKET;
    }
    sfd = socket(AF_INET6, SOCK_STREAM, 0);
    if (!isValidSocketHandle(sfd)) {
        WSALOG_E("Failed to create socket");
        return INVALID_SOCKET;
    }

    name.sin6_family = AF_INET6;
    name.sin6_port = htons(kTgBotHostPort);
    name.sin6_addr = in6addr_any;
    if (bind(sfd, reinterpret_cast<struct sockaddr *>(&name), sizeof(name)) !=
        0) {
        WSALOG_E("Failed to bind to socket");
        closesocket(sfd);
        WSACleanup();
        return INVALID_SOCKET;
    }
    return sfd;
}

SocketInterfaceWindowsIPv6::socket_handle_t
SocketInterfaceWindowsIPv6::createClientSocket() {
    struct sockaddr_in name {};
    socket_handle_t sfd;
    WSADATA data;

    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        WSALOG_E("WSAStartup failed");
        return INVALID_SOCKET;
    }
    sfd = socket(AF_INET6, SOCK_STREAM, 0);
    if (!isValidSocketHandle(sfd)) {
        WSALOG_E("Failed to create socket");
        return INVALID_SOCKET;
    }

    name.sin_family = AF_INET6;
    name.sin_port = htons(kTgBotHostPort);
    InetPton(AF_INET6, getOptions(Options::DESTINATION_ADDRESS).c_str(),
             &name.sin_addr);
    if (connect(sfd, reinterpret_cast<struct sockaddr *>(&name),
                sizeof(name)) != 0) {
        WSALOG_E("Failed to connect to socket");
        closesocket(sfd);
        WSACleanup();
        return INVALID_SOCKET;
    }
    return sfd;
}

bool SocketInterfaceWindowsIPv6::isAvailable() {
    return SocketHelperCommon::isAvailableIPv6(this);
}