#include <absl/log/log.h>
#include <fmt/chrono.h>
#include <tgbot/TgException.h>
#include <tgbot/tools/StringTools.h>
#include <trivial_helpers/_std_chrono_templates.h>

#include <algorithm>
#include <chrono>
#include <fstream>

#include "DataStructParsers.hpp"
#include "PacketParser.hpp"
#include "PacketUtils.hpp"
#include "SocketInterface.hpp"

using TgBot::InputFile;
using namespace TgBotSocket;
using namespace TgBotSocket::callback;

GenericAck SocketInterfaceTgBot::handle_WriteMsgToChatId(
    const std::uint8_t* ptr, TgBotSocket::Packet::Header::length_type len,
    TgBotSocket::PayloadType type) {
    const auto data = WriteMsgToChatId::fromBuffer(ptr, len, type);
    if (!data) {
        return GenericAck(AckType::ERROR_INVALID_ARGUMENT, "Invalid command");
    }
    try {
        api->sendMessage(data->chat, data->message);
    } catch (const TgBot::TgException& e) {
        LOG(ERROR) << "Exception at handler: " << e.what();
        return GenericAck(AckType::ERROR_TGAPI_EXCEPTION, e.what());
    }
    return GenericAck::ok();
}

GenericAck SocketInterfaceTgBot::handle_CtrlSpamBlock(
    const std::uint8_t* ptr) {
    const auto* data =
        reinterpret_cast<const TgBotSocket::data::CtrlSpamBlock*>(ptr);
    spamblock->setConfig(*data);
    return GenericAck::ok();
}

GenericAck SocketInterfaceTgBot::handle_ObserveChatId(
    const std::uint8_t* ptr, TgBotSocket::Packet::Header::length_type len,
    TgBotSocket::PayloadType type) {
    auto data = ObserveChatId::fromBuffer(ptr, len, type);

    if (!data) {
        return GenericAck(AckType::ERROR_INVALID_ARGUMENT, "Invalid command");
    }
    bool observe = data->observe;
    if (!observer->observeAll(true)) {
        return GenericAck(AckType::ERROR_COMMAND_IGNORED,
                          "CMD_OBSERVE_ALL_CHATS active");
    } else {
        observer->observeAll(false);
    }
    if (observe) {
        if (observer->startObserving(data->chat)) {
            LOG(INFO) << "Observing chat '" << data->chat << "'";
        } else {
            return GenericAck(AckType::ERROR_COMMAND_IGNORED,
                              "Target chat was already being observed");
        }
    } else {
        if (observer->stopObserving(data->chat)) {
            LOG(INFO) << "Stopped observing chat '" << data->chat << "'";
        } else {
            return GenericAck(AckType::ERROR_COMMAND_IGNORED,
                              "Target chat wasn't being observed");
        }
    }
    return GenericAck::ok();
}

GenericAck SocketInterfaceTgBot::handle_SendFileToChatId(
    const std::uint8_t* ptr, TgBotSocket::Packet::Header::length_type len,
    TgBotSocket::PayloadType type) {
    const auto data = SendFileToChatId::fromBuffer(ptr, len, type);
    if (!data) {
        return GenericAck(AckType::ERROR_INVALID_ARGUMENT, "Invalid command");
    }
    const auto file = data->filePath.string();
    if (data->filePath.empty()) {
        return GenericAck(AckType::ERROR_INVALID_ARGUMENT, "No file provided");
    }
    std::function<Message::Ptr(ChatId, const TgBotApi::FileOrMedia&)> fn;
    switch (data->fileType) {
        using FileType = TgBotSocket::data::FileType;
        case FileType::TYPE_PHOTO:
            fn = [this](ChatId id, const TgBotApi::FileOrMedia& file) {
                return api->sendPhoto(id, file);
            };
            break;
        case FileType::TYPE_VIDEO:
            fn = [this](ChatId id, const TgBotApi::FileOrMedia& file) {
                return api->sendVideo(id, file);
            };
            break;
        case FileType::TYPE_GIF:
            fn = [this](ChatId id, const TgBotApi::FileOrMedia& file) {
                return api->sendAnimation(id, file);
            };
            break;
        case FileType::TYPE_DOCUMENT:
            fn = [this](ChatId id, const TgBotApi::FileOrMedia& file) {
                return api->sendDocument(id, file);
            };
            break;
        case FileType::TYPE_STICKER:
            fn = [this](ChatId id, const TgBotApi::FileOrMedia& file) {
                return api->sendSticker(id, file);
            };
            break;
        case FileType::TYPE_DICE: {
            api->sendDice(data->chat);
            return GenericAck::ok();
        }
        default:
            fn = [data](ChatId, const TgBotApi::FileOrMedia&) {
                throw TgBot::TgException(
                    "Invalid file type: " +
                        std::to_string(static_cast<int>(data->fileType)),
                    TgBot::TgException::ErrorCode::Undefined);
                return nullptr;
            };
            break;
    }
    DLOG(INFO) << "Sending " << file << " to " << data->chat;
    // Try to send as local file first
    try {
        fn(data->chat,
           InputFile::fromFile(file, getMIMEType(resource, file)));
    } catch (const std::ifstream::failure& e) {
        LOG(INFO) << "Failed to send '" << file
                  << "' as local file, trying as Telegram "
                     "file id";
        MediaIds ids{};
        ids.id = file;
        try {
            fn(data->chat, ids);
        } catch (const TgBot::TgException& e) {
            LOG(ERROR) << "Exception at handler, " << e.what();
            return GenericAck(AckType::ERROR_TGAPI_EXCEPTION, e.what());
        }
    }
    return GenericAck::ok();
}

GenericAck SocketInterfaceTgBot::handle_ObserveAllChats(
    const std::uint8_t* ptr, TgBotSocket::Packet::Header::length_type len,
    TgBotSocket::PayloadType type) {
    auto data = ObserveAllChats::fromBuffer(ptr, len, type);
    if (!data) {
        return GenericAck(AckType::ERROR_INVALID_ARGUMENT, "Invalid command");
    }
    observer->observeAll(data->observe);
    return GenericAck::ok();
}

GenericAck SocketInterfaceTgBot::handle_TransferFile(
    const std::uint8_t* ptr, TgBotSocket::Packet::Header::length_type len,
    TgBotSocket::PayloadType type) {
    const auto f = TransferFileMeta::fromBuffer(ptr, len, type);
    if (!f) {
        return GenericAck(AckType::ERROR_INVALID_ARGUMENT,
                          "Cannot parse TransferFileMeta");
    }

    if (!helper->ReceiveTransferMeta(*f)) {
        return GenericAck(AckType::ERROR_COMMAND_IGNORED,
                          "Options verification failed");
    } else {
        return GenericAck::ok();
    }
}

std::optional<Packet> SocketInterfaceTgBot::handle_TransferFileRequest(
    const std::uint8_t* ptr, TgBotSocket::Packet::Header::length_type len,
    const TgBotSocket::Packet::Header::session_token_type& token,
    TgBotSocket::PayloadType type) {
    auto f = TransferFileMeta::fromBuffer(ptr, len, type);
    if (!f) {
        return toJSONPacket(GenericAck(AckType::ERROR_INVALID_ARGUMENT,
                                       "Cannot parse UploadFileMeta"),
                            token);
    }

    // Since a request is made, we need to send the file
    f->options.dry_run = false;

    return helper->CreateTransferMeta(*f, token, type, false);
}

std::optional<Packet> SocketInterfaceTgBot::handle_GetUptime(
    const TgBotSocket::Packet::Header::session_token_type& token,
    TgBotSocket::PayloadType type) {
    auto now = std::chrono::system_clock::now();
    const auto diff = to_secs(now - startTp);

    switch (type) {
        case PayloadType::Binary: {
            GetUptimeCallback callback{};
            copyTo(callback.uptime, fmt::format("Uptime: {:%H:%M:%S}", diff));
            LOG(INFO) << "Sending text back: "
                      << std::quoted(callback.uptime.data());
            return createPacket(Command::CMD_GET_UPTIME_CALLBACK, &callback,
                                sizeof(callback), PayloadType::Binary, token);
        } break;
        case PayloadType::Json: {
            nlohmann::json payload;
            payload["start_time"] = fmt::format("{}", startTp);
            payload["current_time"] = fmt::format("{}", now);
            payload["uptime"] = fmt::format("{:%Hh %Mm %Ss}", diff);
            LOG(INFO) << "Sending JSON back: " << payload.dump(2);
            return nodeToPacket(Command::CMD_GET_UPTIME_CALLBACK, payload,
                                token);
        } break;
        default:
            LOG(WARNING) << "Unsupported payload type: "
                         << static_cast<int>(type);
    }
    return std::nullopt;
}