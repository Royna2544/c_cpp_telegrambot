#include <impl/SocketWindows.hpp>

#include "socket/TgBotSocket.h"

// clang-format off
#include <winsock2.h>
#include <winsock.h>
#include <afunix.h>
// clang-format on

#include <CStringLifetime.h>

#include <libos/libfs.hpp>

socket_handle_t SocketInterfaceWindowsLocal::makeSocket(bool is_client) {
    struct sockaddr_un name {};
    auto *_name = reinterpret_cast<struct sockaddr *>(&name);
    CStringLifetime path = getOptions(Options::DESTINATION_ADDRESS);
    SOCKET fd;

    if (!is_client) {
        LOG(INFO) << "Creating socket at " << path.get();
    }

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == INVALID_SOCKET) {
        WSALOG_E("Failed to create socket");
        return INVALID_SOCKET;
    }

    name.sun_family = AF_UNIX;
    strncpy(name.sun_path, path.get(), sizeof(name.sun_path));
    name.sun_path[sizeof(name.sun_path) - 1] = '\0';

    decltype(&connect) fn = is_client ? connect : bind;
    if (fn(fd, _name, sizeof(name)) != 0) {
        int err = WSAGetLastError();
        LOG(ERROR) << "Failed to " << (is_client ? "connect" : "bind")
                   << " to socket: " << strWSAError(err);
        do {
            if (!is_client && err == WSAEADDRINUSE) {
                cleanupServerSocket();
                if (bind(fd, _name, sizeof(name)) == 0) {
                    LOG(INFO) << "Bind succeeded by removing socket file";
                    break;
                } else {
                    err = WSAGetLastError();
                    LOG(ERROR)
                        << "Failed to bind even after removing socket file: "
                        << strWSAError(err);
                }
            }
            closesocket(fd);
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
    helper.local.cleanupServerSocket();
}

bool SocketInterfaceWindowsLocal::canSocketBeClosed() {
    return helper.local.canSocketBeClosed();
}

bool SocketInterfaceWindowsLocal::isSupported() {
    return helper.local.isSupported();
}

void SocketInterfaceWindowsLocal::doGetRemoteAddr(socket_handle_t s) {}