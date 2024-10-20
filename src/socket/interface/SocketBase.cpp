#include "SocketBase.hpp"

#include <absl/log/log.h>

#include <optional>
#include <utility>

#include "SharedMalloc.hpp"

void SocketInterfaceBase::writeAsClientToSocket(SharedMalloc data) {
    const auto handle = createClientSocket();
    if (handle.has_value()) {
        writeToSocket(handle.value(), std::move(data));
    }
}

void SocketInterfaceBase::startListeningAsServer(
    const listener_callback_t onNewData) {
    auto hdl = createServerSocket();
    if (hdl) {
        startListening(hdl.value(), onNewData);
    }
}

bool SocketInterfaceBase::closeSocketHandle(SocketConnContext& context) {
    return closeSocketHandle(context.cfd);
}

// Required for Windows
constexpr int SocketInterfaceBase::kTgBotHostPort; // NOLINT
constexpr int SocketInterfaceBase::kTgBotLogPort; // NOLINT