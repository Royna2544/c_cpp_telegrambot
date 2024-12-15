#pragma once

#include <SocketExports.h>

#include <SocketContext.hpp>
#include <optional>

namespace TgBotSocket {

std::optional<Packet> Socket_API readPacket(const TgBotSocket::Context& context);

}