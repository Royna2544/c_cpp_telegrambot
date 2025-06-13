#include <absl/log/log.h>

#include "SocketContext.hpp"

namespace TgBotSocket {

Context::Context() = default;

Context::~Context() = default;

bool Context::write(const Packet& packet) const {
    return write(reinterpret_cast<const uint8_t*>(&packet.header),
                 sizeof(packet.header)) &&
           write(packet.data) &&
           write(reinterpret_cast<const uint8_t*>(packet.hmac.data()),
                 packet.hmac.size());
}

bool Context::write(const SharedMalloc& data) const {
    if (data.size() == 0) return true;
    return write(data.get(), data.size());
}

constexpr int Context::kTgBotHostPort;
constexpr int Context::kTgBotLogPort;
constexpr int Context::kTgBotLogTransmitPort;

SOCKET_EXPORT std::ostream& operator<<(std::ostream& stream,
                                    const Context::RemoteEndpoint& endpoint) {
    stream << endpoint.address << ":" << endpoint.port;
    return stream;
}

}  // namespace TgBotSocket
