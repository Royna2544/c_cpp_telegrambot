#include "../SocketInterfaceWindows.h"
#include "Logging.h"
#include "socket/SocketInterfaceBase.h"
#include "socket/TgBotSocket.h"

// clang-format off
#include <winsock2.h>
#include <winsock.h>
#include <afunix.h>
// clang-format on

#include <CStringLifetime.h>
#include <libos/libfs.hpp>

socket_handle_t SocketInterfaceWindowsLocal::makeSocket(
    bool is_client) {
    struct sockaddr_un name {};
    CStringLifetime path = getOptions(Options::DESTINATION_ADDRESS);
    WSADATA data;
    SOCKET fd;
    int ret;

    if (!is_client) {
        LOG(LogLevel::DEBUG, "Creating socket at %s", path.get());
    }

    ret = WSAStartup(MAKEWORD(2, 2), &data);
    if (ret != 0) {
        LOG(LogLevel::ERROR, "WSAStartup failed");
        return INVALID_SOCKET;
    }
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == INVALID_SOCKET) {
        WSALOG_E("Failed to create socket");
        WSACleanup();
        return INVALID_SOCKET;
    }

    name.sun_family = AF_UNIX;
    strncpy(name.sun_path, path.get(), sizeof(name.sun_path));
    name.sun_path[sizeof(name.sun_path) - 1] = '\0';

    decltype(&connect) fn = is_client ? connect : bind;
    if (fn(fd, reinterpret_cast<struct sockaddr *>(&name), sizeof(name)) != 0) {
        if (is_client)
            WSALOG_E("Failed to connect to socket");
        else
            WSALOG_E("Failed to bind to socket");
        do {
            if (!is_client && WSAGetLastError() == WSAEADDRINUSE) {
                cleanupServerSocket();
                if (fn(fd, reinterpret_cast<struct sockaddr *>(&name), sizeof(name)) == 0) {
                    LOG(LogLevel::INFO, "Bind succeeded by removing socket file");
                    break;
                }
            }
            closesocket(fd);
            WSACleanup();
            return INVALID_SOCKET;
        } while (false);
    }
    return fd;
}

socket_handle_t SocketInterfaceWindowsLocal::createClientSocket() {
    setOptions(Options::DESTINATION_ADDRESS, getSocketPath().string());
    return makeSocket(/*is_client=*/true);
}

socket_handle_t SocketInterfaceWindowsLocal::createServerSocket() {
    setOptions(Options::DESTINATION_ADDRESS, getSocketPath().string());
    return makeSocket(/*is_client=*/false);
}

void SocketInterfaceWindowsLocal::cleanupServerSocket() {
    SocketInterfaceWindows::cleanupServerSocket();
    SocketHelperCommon::cleanupServerSocketLocalSocket(this);
}

bool SocketInterfaceWindowsLocal::canSocketBeClosed() {
    return SocketHelperCommon::canSocketBeClosedLocalSocket(this);
}

bool SocketInterfaceWindowsLocal::isAvailable() {
    return SocketHelperCommon::isAvailableLocalSocket();
}