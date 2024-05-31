#include <impl/SocketWindows.hpp>

#include "SocketBase.hpp"
#include "socket/TgBotSocket.h"

// clang-format off
#include <winsock2.h>
#include <winsock.h>
#include <afunix.h>
// clang-format on

#include <CStringLifetime.h>

#include <libos/libfs.hpp>

bool SocketInterfaceWindowsLocal::createLocalSocket(SocketConnContext *ctx) {
    ctx->cfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (ctx->cfd < 0) {
        LOG(ERROR) << "Failed to create socket: " << WSALastErrorStr();
        return false;
    }
    auto *addr = reinterpret_cast<struct sockaddr_un *>(ctx->addr.get());
    addr->sun_family = AF_UNIX;
    strncpy(addr->sun_path, getOptions(Options::DESTINATION_ADDRESS).c_str(),
            sizeof(addr->sun_path));
    addr->sun_path[sizeof(addr->sun_path) - 1] = '\0';
    return true;
}

std::optional<socket_handle_t> SocketInterfaceWindowsLocal::createServerSocket() {
    SocketConnContext ret = SocketConnContext::create<sockaddr_un>();
    const auto *_name = reinterpret_cast<struct sockaddr *>(ret.addr.get());

    setOptions(Options::DESTINATION_ADDRESS, getSocketPath().string());
    LOG(INFO) << "Creating socket at " << getSocketPath().string();
    if (!createLocalSocket(&ret)) {
        return std::nullopt;
    }
    if (bind(ret.cfd, _name, ret.addr->size) != 0) {
        bool succeeded = false;
        LOG(ERROR) << "Failed to bind to socket: " << WSALastErrorStr();
        if (WSAGetLastError() == WSAEADDRINUSE) {
            cleanupServerSocket();
            if (bind(ret.cfd, _name, ret.addr->size) == 0) {
                LOG(INFO) << "Bind succeeded by removing socket file";
                succeeded = true;
            }
        }
        if (!succeeded) {
            closeSocketHandle(ret.cfd);
            return std::nullopt;
        }
    }
    return ret.cfd;
}

std::optional<SocketConnContext> SocketInterfaceWindowsLocal::createClientSocket() {
    SocketConnContext ret = SocketConnContext::create<sockaddr_un>();
    const auto *_name = reinterpret_cast<struct sockaddr *>(ret.addr.get());

    setOptions(Options::DESTINATION_ADDRESS, getSocketPath().string());
    if (!createLocalSocket(&ret)) {
        return std::nullopt;
    }
    if (connect(ret.cfd, _name, ret.addr->size) != 0) {
        LOG(ERROR) << "Failed to connect to socket: " << WSALastErrorStr();
        closeSocketHandle(ret.cfd);
        return std::nullopt;
    }
    return ret;
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

void SocketInterfaceWindowsLocal::doGetRemoteAddr(socket_handle_t s) {
    helper.local.doGetRemoteAddr(s);
}