#include <absl/log/log.h>

#include "SocketContext.hpp"

namespace TgBotSocket {

Context::Context() = default;

Context::~Context() = default;

bool Context::write(const Packet& packet) const {
    auto data = packet.data;
    auto header = packet.header;
    // Converts to full SocketData object, including header
    data.resize(sizeof(Packet::Header) + header.data_size);
    data.move(0, sizeof(header), header.data_size);
    data.assignFrom(header);
    return write(data);
}

bool Context::write(const SharedMalloc& data) const {
    return write(data.get(), data.size());
}

constexpr int Context::kTgBotHostPort;
constexpr int Context::kTgBotLogPort;
constexpr int Context::kTgBotLogTransmitPort;

Socket_API std::ostream& operator<<(std::ostream& stream, const Context::RemoteEndpoint& endpoint) {
    stream << endpoint.address << ":" << endpoint.port;
    return stream;
}

}  // namespace TgBotSocket