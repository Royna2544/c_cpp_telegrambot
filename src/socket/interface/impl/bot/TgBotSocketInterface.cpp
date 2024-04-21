#include "TgBotSocketInterface.hpp"

#include <new>
#include <optional>

#include "SharedMalloc.hpp"
#include "socket/TgBotSocket.h"

using HandleState = SocketInterfaceTgBot::HandleState;

HandleState SocketInterfaceTgBot::handle_PacketHeader(
    std::optional<SocketData>& socketData,
    std::optional<TgBotCommandPacket>& pkt) {
    if (socketData->len < TgBotCommandPacket::hdr_sz) {
        LOG(ERROR) << "Failed to read from socket";
        return HandleState::Fail;
    }
    pkt = TgBotCommandPacket(TgBotCommandPacket::hdr_sz);
    memcpy(&pkt->header, socketData->data->getData(),
           TgBotCommandPacket::hdr_sz);

    if (pkt->header.magic != TgBotCommandPacket::MAGIC_VALUE) {
        LOG(WARNING) << "Invalid magic value, dropping buffer";
        return HandleState::Ignore;
    }

    return HandleState::Ok;
}

HandleState SocketInterfaceTgBot::handle_Packet(
    std::optional<SocketData>& socketData,
    std::optional<TgBotCommandPacket>& pkt) {
    if (TgBotCmd::isClientCommand(pkt->header.cmd)) {
        LOG(INFO) << "Received buf with " << TgBotCmd::toStr(pkt->header.cmd)
                  << ", invoke callback!";
    }

    if (pkt->header.cmd == CMD_EXIT) {
        return HandleState::Fail;
    }
    if (socketData->len != pkt->header.data_size) {
        LOG(WARNING) << "Invalid packet data size, dropping buffer";
        return HandleState::Ignore;
    }
    pkt->data_ptr->set_size(TgBotCommandPacket::hdr_sz + pkt->header.data_size);
    pkt->data_ptr->alloc();
    memcpy(pkt->data_ptr.getData(), socketData->data->getData(),
           pkt->header.data_size);

    return HandleState::Ok;
}

void SocketInterfaceTgBot::runFunction() {
    interface->startListening(
        [this](SocketInterfaceBase*, socket_handle_t cfd) {
            return onNewBuffer(_bot, cfd);
        });
}

bool SocketInterfaceTgBot::onNewBuffer(const Bot& bot, socket_handle_t cfd) {
    std::optional<SocketData> data;
    std::optional<TgBotCommandPacket> pkt;
    bool ret = false;

    data = interface->readFromSocket(cfd, sizeof(TgBotCommandPacketHeader));
    switch (handle_PacketHeader(data, pkt)) {
        case HandleState::Ok: {
            data = interface->readFromSocket(cfd, pkt->header.data_size);
            switch (handle_Packet(data, pkt)) {
                case HandleState::Ok: {
                    socketConnectionHandler(bot, pkt.value());
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
