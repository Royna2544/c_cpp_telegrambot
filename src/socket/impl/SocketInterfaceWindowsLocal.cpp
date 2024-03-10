#include "../SocketInterfaceWindows.h"

// clang-format off
#include <winsock2.h>
#include <winsock.h>
#include <afunix.h>
// clang-format on

#include <CStringLifetime.h>
#include <FileSystemLib.h>
#include <filesystem>

SocketInterfaceWindows::socket_handle_t
SocketInterfaceWindowsLocal::makeSocket(bool is_client) {
    struct sockaddr_un name {};
    CStringLifetime path = getOptions(Options::DESTINATION_ADDRESS);
    WSADATA data;
    SOCKET fd;
    int ret;

    if (!is_client) {
        LOG_D("Creating socket at %s", path.get());
        std::filesystem::remove(path.get());
    }

    ret = WSAStartup(MAKEWORD(2, 2), &data);
    if (ret != 0) {
        LOG_E("WSAStartup failed");
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
        WSALOG_E("Failed to %s to socket", is_client ? "connect" : "bind");
        closesocket(fd);
        WSACleanup();
        return INVALID_SOCKET;
    }
    return fd;
}

bool SocketInterfaceWindowsLocal::canSocketBeClosed() {
    return !fileExists(SOCKET_PATH);
}

SocketInterfaceWindows::socket_handle_t
SocketInterfaceWindowsLocal::createClientSocket() {
    setOptions(Options::DESTINATION_ADDRESS, SOCKET_PATH);
    return makeSocket(/*is_client=*/true);
}

SocketInterfaceWindows::socket_handle_t
SocketInterfaceWindowsLocal::createServerSocket() {
    setOptions(Options::DESTINATION_ADDRESS, SOCKET_PATH);
    return makeSocket(/*is_client=*/false);
}

SocketInterfaceWindows::socket_handle_t
SocketInterfaceWindowsLocal::createClientInternalSocket() {
    setOptions(Options::DESTINATION_ADDRESS, INTERNAL_SOCKET_PATH);
    return makeSocket(/*is_client=*/true);
}

SocketInterfaceWindows::socket_handle_t
SocketInterfaceWindowsLocal::createServerInternalSocket() {
    setOptions(Options::DESTINATION_ADDRESS, INTERNAL_SOCKET_PATH);
    return makeSocket(/*is_client=*/false);
}

void SocketInterfaceWindowsLocal::cleanupServerSocket() {
    std::filesystem::remove(SOCKET_PATH);
}