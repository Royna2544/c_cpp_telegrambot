#pragma once

#include <SocketBase.hpp>
#include <TgBotSocket_Export.hpp>
#include <optional>

namespace TgBotSocket {

std::optional<Packet> readPacket(
    const std::shared_ptr<SocketInterfaceBase> &interface,
    const SocketConnContext &context);
    
}