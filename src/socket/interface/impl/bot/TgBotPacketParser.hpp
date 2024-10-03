#pragma once

#include <TgBotSocketExports.h>

#include <SocketBase.hpp>
#include <optional>

namespace TgBotSocket {

std::optional<Packet> TgBotSocket_API
readPacket(const std::shared_ptr<SocketInterfaceBase> &interface,
           const SocketConnContext &context);

}