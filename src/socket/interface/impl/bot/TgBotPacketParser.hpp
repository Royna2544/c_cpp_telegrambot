#pragma once

#include <TgBotSocketExports.h>

#include <SocketBase.hpp>
#include <optional>

namespace TgBotSocket {

std::optional<Packet> TgBotSocket_API
readPacket(SocketInterfaceBase* interface, const SocketConnContext& context);

}