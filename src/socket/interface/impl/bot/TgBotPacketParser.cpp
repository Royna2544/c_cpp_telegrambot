#include "TgBotPacketParser.hpp"

#include <absl/log/log.h>

#include <SocketBase.hpp>
#include <TgBotSocket_Export.hpp>
#include <optional>
#include <socket/TgBotCommandMap.hpp>
#include <utility>

namespace TgBotSocket {

std::optional<Packet> readPacket(
    const std::shared_ptr<SocketInterfaceBase>& interface,
    const SocketConnContext& context) {
    TgBotSocket::PacketHeader header;
    decltype(header.magic) magic{};

    // Read header and check magic value, despite the header size, the magic was
    // always the first element of the struct.
    const auto magicData = interface->readFromSocket(context, sizeof(magic));
    if (!magicData) {
        LOG(ERROR) << "While reading magic, failed";
        return std::nullopt;
    }
    magicData->assignTo(magic);

    auto diff = magic - TgBotSocket::PacketHeader::MAGIC_VALUE_BASE;
    if (diff != TgBotSocket::PacketHeader::DATA_VERSION) {
        LOG(WARNING) << "Invalid magic value, dropping buffer";
        constexpr int reasonable_datadiff = 5;
        // Only a small difference is worth logging.
        diff = abs(diff);
        if (diff >= 0 && diff < TgBotSocket::PacketHeader::DATA_VERSION +
                                    reasonable_datadiff) {
            LOG(INFO) << "This packet contains header data version " << diff
                      << ", but we have version "
                      << TgBotSocket::PacketHeader::DATA_VERSION;
        } else {
            LOG(INFO) << "This is probably not a valid packet";
        }
        return std::nullopt;
    }

    // Read rest of the packet header.
    const auto headerData = interface->readFromSocket(
        context, sizeof(TgBotSocket::PacketHeader) - sizeof(magic));
    if (!headerData) {
        LOG(ERROR) << "While reading header, failed";
        return std::nullopt;
    }
    headerData->assignTo((decltype(magic) *)&header+1, sizeof(header) - sizeof(magic));
    header.magic = magic;

    using namespace TgBotSocket::CommandHelpers;
    if (isClientCommand(header.cmd)) {
        LOG(INFO) << "Received buf with " << toStr(header.cmd)
                  << ", invoke callback!";
    }

    const size_t newLength =
        sizeof(TgBotSocket::PacketHeader) + header.data_size;
    TgBotSocket::Packet packet(newLength);

    auto data = interface->readFromSocket(context, header.data_size);
    if (!data) {
        LOG(ERROR) << "While reading data, failed";
        return std::nullopt;
    }
    if (header.checksum != Packet::crc32_function(data.value())) {
        LOG(WARNING) << "Checksum mismatch, dropping buffer";
        return std::nullopt;
    }
    packet.data = std::move(data.value());
    packet.header = header;
    return packet;
}
}  // namespace TgBotSocket