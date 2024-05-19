#include "TgBotPacketParser.hpp"

#include <absl/log/log.h>

#include "SharedMalloc.hpp"
#include "SocketBase.hpp"

using HandleState = TgBotSocketParser::HandleState;

HandleState TgBotSocketParser::handle_PacketHeader(
    std::optional<SharedMalloc>& socketData,
    std::optional<TgBotCommandPacket>& pkt) {
    if (socketData.value()->size < TgBotCommandPacket::hdr_sz) {
        LOG(ERROR) << "Failed to read from socket";
        return HandleState::Fail;
    }
    pkt = TgBotCommandPacket(TgBotCommandPacket::hdr_sz);
    memcpy(&pkt->header, socketData->get(), TgBotCommandPacket::hdr_sz);

    if (pkt->header.magic != TgBotCommandPacketHeader::MAGIC_VALUE) {
        LOG(WARNING) << "Invalid magic value, dropping buffer";
        return HandleState::Ignore;
    }

    return HandleState::Ok;
}

HandleState TgBotSocketParser::handle_Packet(
    std::optional<SharedMalloc>& socketData,
    std::optional<TgBotCommandPacket>& pkt) {
    if (TgBotCmd::isClientCommand(pkt->header.cmd)) {
        LOG(INFO) << "Received buf with " << TgBotCmd::toStr(pkt->header.cmd)
                  << ", invoke callback!";
    }

    if (socketData.value()->size != pkt->header.data_size) {
        LOG(WARNING) << "Invalid packet data size, dropping buffer";
        return HandleState::Ignore;
    }
    pkt->data->size = TgBotCommandPacket::hdr_sz + pkt->header.data_size;
    pkt->data->alloc();
    memcpy(pkt->data.get(), socketData->get(), pkt->header.data_size);

    return HandleState::Ok;
}

bool TgBotSocketParser::onNewBuffer(SocketConnContext ctx) {
    std::optional<SharedMalloc> data;
    std::optional<TgBotCommandPacket> pkt;
    bool ret = false;

    data = interface->readFromSocket(ctx, sizeof(TgBotCommandPacketHeader));
    switch (handle_PacketHeader(data, pkt)) {
        case HandleState::Ok: {
            data = interface->readFromSocket(ctx, pkt->header.data_size);
            switch (handle_Packet(data, pkt)) {
                case HandleState::Ok: {
                    handle_CommandPacket(ctx, pkt.value());
                    break;
                }
                case HandleState::Ignore: {
                    break;
                }
                case HandleState::Fail: {
                    ret = true;
                    break;
                }
            }
            [[fallthrough]];
        }
        case HandleState::Ignore:
            ret = false;
            break;
        case HandleState::Fail:
            LOG(ERROR) << "Failed to handle packet header";
            ret = true;
            break;
    }
    return ret;
}
