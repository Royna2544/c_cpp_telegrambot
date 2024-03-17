#include <winsock2.h>
#include <ws2tcpip.h>

#include "../SocketInterfaceWindows.h"

socket_handle_t SocketInterfaceWindowsIPv4::createServerSocket() {
    struct sockaddr_in name {};
    socket_handle_t sfd;
    WSADATA data;

    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        WSALOG_E("WSAStartup failed");
        return INVALID_SOCKET;
    }
    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (!isValidSocketHandle(sfd)) {
        WSALOG_E("Failed to create socket");
        return INVALID_SOCKET;
    }

    name.sin_family = AF_INET;
    name.sin_port = htons(kTgBotHostPort);
    name.sin_addr.s_addr = INADDR_ANY;
    if (bind(sfd, reinterpret_cast<struct sockaddr *>(&name), sizeof(name)) !=
        0) {
        WSALOG_E("Failed to bind to socket");
        closesocket(sfd);
        WSACleanup();
        return INVALID_SOCKET;
    }
    return sfd;
}

socket_handle_t SocketInterfaceWindowsIPv4::createClientSocket() {
    struct sockaddr_in name {};
    socket_handle_t sfd;
    WSADATA data;

    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        WSALOG_E("WSAStartup failed");
        return INVALID_SOCKET;
    }
    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (!isValidSocketHandle(sfd)) {
        WSALOG_E("Failed to create socket");
        return INVALID_SOCKET;
    }

    name.sin_family = AF_INET;
    name.sin_port = htons(kTgBotHostPort);
    InetPton(AF_INET, getOptions(Options::DESTINATION_ADDRESS).c_str(),
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

bool SocketInterfaceWindowsIPv4::isAvailable() {
    return SocketHelperCommon::isAvailableIPv4(this);
}