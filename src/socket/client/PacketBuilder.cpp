#include "PacketBuilder.hpp"
#include "CommandParser.hpp"

#include <absl/log/log.h>
#include <api/Commands.hpp>

// fmt::formatter specialization for Command enum
template <>
struct fmt::formatter<TgBotSocket::Command>
    : fmt::formatter<::std::string_view> {
    auto format(TgBotSocket::Command cmd, format_context& ctx) const
        -> format_context::iterator {
        ::std::string_view name = "UNKNOWN";
        switch (cmd) {
            case TgBotSocket::Command::CMD_INVALID:
                name = "CMD_INVALID";
                break;
            case TgBotSocket::Command::CMD_WRITE_MSG_TO_CHAT_ID:
                name = "CMD_WRITE_MSG_TO_CHAT_ID";
                break;
            case TgBotSocket::Command::CMD_CTRL_SPAMBLOCK:
                name = "CMD_CTRL_SPAMBLOCK";
                break;
            case TgBotSocket::Command::CMD_OBSERVE_CHAT_ID:
                name = "CMD_OBSERVE_CHAT_ID";
                break;
            case TgBotSocket::Command::CMD_SEND_FILE_TO_CHAT_ID:
                name = "CMD_SEND_FILE_TO_CHAT_ID";
                break;
            case TgBotSocket::Command::CMD_OBSERVE_ALL_CHATS:
                name = "CMD_OBSERVE_ALL_CHATS";
                break;
            case TgBotSocket::Command::CMD_GET_UPTIME:
                name = "CMD_GET_UPTIME";
                break;
            case TgBotSocket::Command::CMD_TRANSFER_FILE:
                name = "CMD_TRANSFER_FILE";
                break;
            case TgBotSocket::Command::CMD_TRANSFER_FILE_REQUEST:
                name = "CMD_TRANSFER_FILE_REQUEST";
                break;
            case TgBotSocket::Command::CMD_TRANSFER_FILE_BEGIN:
                name = "CMD_TRANSFER_FILE_BEGIN";
                break;
            case TgBotSocket::Command::CMD_TRANSFER_FILE_CHUNK:
                name = "CMD_TRANSFER_FILE_CHUNK";
                break;
            case TgBotSocket::Command::CMD_TRANSFER_FILE_CHUNK_RESPONSE:
                name = "CMD_TRANSFER_FILE_CHUNK_RESPONSE";
                break;
            case TgBotSocket::Command::CMD_TRANSFER_FILE_END:
                name = "CMD_TRANSFER_FILE_END";
                break;
            case TgBotSocket::Command::CMD_GET_UPTIME_CALLBACK:
                name = "CMD_GET_UPTIME_CALLBACK";
                break;
            case TgBotSocket::Command::CMD_GENERIC_ACK:
                name = "CMD_GENERIC_ACK";
                break;
            case TgBotSocket::Command::CMD_OPEN_SESSION:
                name = "CMD_OPEN_SESSION";
                break;
            case TgBotSocket::Command::CMD_OPEN_SESSION_ACK:
                name = "CMD_OPEN_SESSION_ACK";
                break;
            case TgBotSocket::Command::CMD_CLOSE_SESSION:
                name = "CMD_CLOSE_SESSION";
                break;
            case TgBotSocket::Command::CMD_MAX:
                name = "CMD_MAX";
                break;
        }
        return fmt::formatter<::std::string_view>::format(name, ctx);
    }
};

namespace TgBotSocket::Client {

std::optional<Packet> PacketBuilder::buildWriteMsg(
    char** argv, const Packet::Header::session_token_type& token) {
    auto args = CommandParser::parseWriteMsg(argv);
    if (!args) {
        return std::nullopt;
    }
    return createPacket(Command::CMD_WRITE_MSG_TO_CHAT_ID, 
                       &args.value(), sizeof(*args),
                       PayloadType::Binary, token);
}

std::optional<Packet> PacketBuilder::buildCtrlSpamBlock(
    char** argv, const Packet::Header::session_token_type& token) {
    auto args = CommandParser::parseCtrlSpamBlock(argv);
    if (!args) {
        return std::nullopt;
    }
    return createPacket(Command::CMD_CTRL_SPAMBLOCK,
                       &args.value(), sizeof(*args),
                       PayloadType::Binary, token);
}

std::optional<Packet> PacketBuilder::buildObserveChat(
    char** argv, const Packet::Header::session_token_type& token) {
    auto args = CommandParser::parseObserveChat(argv);
    if (!args) {
        return std::nullopt;
    }
    return createPacket(Command::CMD_OBSERVE_CHAT_ID,
                       &args.value(), sizeof(*args),
                       PayloadType::Binary, token);
}

std::optional<Packet> PacketBuilder::buildSendFile(
    char** argv, const Packet::Header::session_token_type& token) {
    auto args = CommandParser::parseSendFile(argv);
    if (!args) {
        return std::nullopt;
    }
    return createPacket(Command::CMD_SEND_FILE_TO_CHAT_ID,
                       &args.value(), sizeof(*args),
                       PayloadType::Binary, token);
}

std::optional<Packet> PacketBuilder::buildObserveAll(
    char** argv, const Packet::Header::session_token_type& token) {
    auto args = CommandParser::parseObserveAll(argv);
    if (!args) {
        return std::nullopt;
    }
    return createPacket(Command::CMD_OBSERVE_ALL_CHATS,
                       &args.value(), sizeof(*args),
                       PayloadType::Binary, token);
}

std::optional<Packet> PacketBuilder::buildGetUptime(
    const Packet::Header::session_token_type& token) {
    return createPacket(Command::CMD_GET_UPTIME, nullptr, 0,
                       PayloadType::Binary, token);
}

std::optional<Packet> PacketBuilder::buildTransferFile(
    Command cmd, char** argv, const Packet::Header::session_token_type& token) {
    RealFS realfs;
    SocketFile2DataHelper helper(&realfs);
    auto args = CommandParser::parseTransferFile(argv);
    if (!args) {
        return std::nullopt;
    }
    return helper.CreateTransferMeta(
        args.value(), token, PayloadType::Json,
        cmd == Command::CMD_TRANSFER_FILE_REQUEST);
}


std::optional<Packet> PacketBuilder::buildPacket(
    Command cmd, char** argv, const Packet::Header::session_token_type& token) {
    switch (cmd) {
        case Command::CMD_WRITE_MSG_TO_CHAT_ID:
            return buildWriteMsg(argv, token);
        case Command::CMD_CTRL_SPAMBLOCK:
            return buildCtrlSpamBlock(argv, token);
        case Command::CMD_OBSERVE_CHAT_ID:
            return buildObserveChat(argv, token);
        case Command::CMD_SEND_FILE_TO_CHAT_ID:
            return buildSendFile(argv, token);
        case Command::CMD_OBSERVE_ALL_CHATS:
            return buildObserveAll(argv, token);
        case Command::CMD_GET_UPTIME:
            return buildGetUptime(token);
        case Command::CMD_TRANSFER_FILE:
        case Command::CMD_TRANSFER_FILE_REQUEST:
            return buildTransferFile(cmd, argv, token);
        default:
            LOG(FATAL) << fmt::format("Unhandled command: {}", cmd);
            return std::nullopt;
    }
}

}  // namespace TgBotSocket::Client