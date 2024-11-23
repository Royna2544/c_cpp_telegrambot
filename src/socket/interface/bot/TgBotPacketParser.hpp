#pragma once

#include <TgBotSocketExports.h>

#include <SocketContext.hpp>
#include <optional>

namespace TgBotSocket {

std::optional<Packet> TgBotSocket_API readPacket(const TgBotSocket::Context& context);

}