#include <absl/log/log.h>
#include <fmt/format.h>

#include <variant>

#include "DataStructParsers.hpp"
#include "PacketParser.hpp"
#include "PacketUtils.hpp"
#include "SocketInterface.hpp"
#include "CommandMap.hpp"

using namespace TgBotSocket;
using namespace TgBotSocket::callback;

void SocketInterfaceTgBot::handlePacket(const TgBotSocket::Context& ctx,
                                        TgBotSocket::Packet pkt) {
    const std::uint8_t* ptr = pkt.data.get();
    std::variant<GenericAck, std::optional<Packet>> ret;
    const auto invalidPacketAck =
        GenericAck(AckType::ERROR_COMMAND_IGNORED, "Invalid packet size");

    switch (pkt.header.cmd.operator Command()) {
        case Command::CMD_WRITE_MSG_TO_CHAT_ID:
            ret = handle_WriteMsgToChatId(ptr, pkt.header.data_size,
                                          pkt.header.data_type);
            break;
        case Command::CMD_CTRL_SPAMBLOCK:
            if (CHECK_PACKET_SIZE<TgBotSocket::data::CtrlSpamBlock>(pkt)) {
                ret = handle_CtrlSpamBlock(ptr);
            } else {
                ret = invalidPacketAck;
            }
            break;
        case Command::CMD_OBSERVE_CHAT_ID:
            ret = handle_ObserveChatId(ptr, pkt.header.data_size,
                                       pkt.header.data_type);
            break;
        case Command::CMD_SEND_FILE_TO_CHAT_ID:
            ret = handle_SendFileToChatId(ptr, pkt.header.data_size,
                                          pkt.header.data_type);
            break;
        case Command::CMD_OBSERVE_ALL_CHATS:
            ret = handle_ObserveAllChats(ptr, pkt.header.data_size,
                                         pkt.header.data_type);
            break;
        case Command::CMD_GET_UPTIME:
            ret = handle_GetUptime(pkt.header.session_token,
                                   pkt.header.data_type);
            break;
        case Command::CMD_TRANSFER_FILE:
            ret = handle_TransferFile(ptr, pkt.header.data_size,
                                      pkt.header.data_type);
            break;
        case Command::CMD_TRANSFER_FILE_REQUEST:
            ret = handle_TransferFileRequest(ptr, pkt.header.data_size,
                                             pkt.header.session_token,
                                             pkt.header.data_type);
            break;
        default:
            if (CommandHelpers::isClientCommand(pkt.header.cmd)) {
                LOG(ERROR) << fmt::format("Unhandled cmd: {}", pkt.header.cmd);
            } else {
                LOG(WARNING) << "cmd ignored (as internal): "
                             << static_cast<int>(pkt.header.cmd);
            }
            return;
    };
    switch (pkt.header.cmd.operator TgBotSocket::Command()) {
        case Command::CMD_GET_UPTIME:
        case Command::CMD_TRANSFER_FILE_REQUEST: {
            // This has its own callback, so we don't need to send ack.
            auto result = std::get<1>(ret);
            LOG_IF(WARNING, (!result))
                << fmt::format("Command failed: {}", pkt.header.cmd);
            if (result) {
                ctx.write(result.value());
                break;
            } else {
                ret = invalidPacketAck;
                [[fallthrough]];
            }
        }
        case Command::CMD_WRITE_MSG_TO_CHAT_ID:
        case Command::CMD_CTRL_SPAMBLOCK:
        case Command::CMD_OBSERVE_CHAT_ID:
        case Command::CMD_SEND_FILE_TO_CHAT_ID:
        case Command::CMD_OBSERVE_ALL_CHATS:
        case Command::CMD_TRANSFER_FILE: {
            GenericAck result = std::get<GenericAck>(ret);
            LOG(INFO) << "Sending ack: " << std::boolalpha
                      << (result.result == AckType::SUCCESS);
            switch (pkt.header.data_type.operator TgBotSocket::PayloadType()) {
                case PayloadType::Binary: {
                    auto ackpkt = createPacket(Command::CMD_GENERIC_ACK,
                                               &result, sizeof(GenericAck),
                                               TgBotSocket::PayloadType::Binary,
                                               pkt.header.session_token);
                    ctx.write(ackpkt);
                } break;
                case PayloadType::Json: {
                    ctx.write(toJSONPacket(result, pkt.header.session_token));
                } break;
            }
            break;
        }
        default:
            LOG(ERROR) << "Unknown command: "
                       << static_cast<int>(pkt.header.cmd);
            break;
    }
}