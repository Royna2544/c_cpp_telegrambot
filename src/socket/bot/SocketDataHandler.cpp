#include <absl/strings/escaping.h>
#include <fmt/chrono.h>
#include <json/json.h>
#include <openssl/sha.h>
#include <tgbot/TgException.h>
#include <tgbot/tools/StringTools.h>
#include <trivial_helpers/_std_chrono_templates.h>

#include <ManagedThreads.hpp>
#include <ResourceManager.hpp>
#include <ApiDef.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <global_handlers/ChatObserver.hpp>
#include <global_handlers/SpamBlock.hpp>
#include <initializer_list>
#include <mutex>
#include <optional>
#include <socket/CommandMap.hpp>
#include <string>
#include <string_view>
#include <trivial_helpers/log_once.hpp>
#include <variant>

#include "FileHelperNew.hpp"
#include "SocketContext.hpp"
#include "SocketInterface.hpp"
#include "bot/PacketParser.hpp"

using TgBot::InputFile;
namespace fs = std::filesystem;
using namespace TgBotSocket;
using namespace TgBotSocket::callback;

namespace {

std::string getMIMEString(const ResourceProvider* resource,
                          const std::string& path) {
    static Json::Value doc;
    std::string extension = fs::path(path).extension().string();

    static bool once = [resource] {
        std::string_view buf;
        buf = resource->get("mimeData.json");
        Json::Reader reader;
        if (!reader.parse(buf.data(), doc)) {
            LOG(ERROR) << "Failed to parse mimedata: "
                       << reader.getFormattedErrorMessages();
        }
        return true;
    }();
    if (!extension.empty()) {
        if (doc.empty()) {
            LOG(ERROR) << "Failed to load mimedata";
            return {};
        }
        // Look for MIME type in json file.
        for (const auto& elem : doc) {
            if (!elem.isMember("types")) {
                continue;
            }
            for (const auto& ex : elem["types"]) {
                if (ex.asString() == extension) {
                    return elem["name"].asString();
                }
            }
        }
        LOG(WARNING) << "Unknown file extension: '" << extension << "'";
    }
    return "application/octet-stream";
}
}  // namespace

Packet toJSONPacket(const GenericAck& ack,
                    const Packet::Header::session_token_type& session_token) {
    Json::Value root;
    root["result"] = ack.result == AckType::SUCCESS;
    if (ack.result != AckType::SUCCESS) {
        root["error_msg"] = safeParse(ack.error_msg);
        switch (ack.result.operator TgBotSocket::callback::AckType()) {
            case AckType::ERROR_CLIENT_ERROR:
                root["error_type"] = "CLIENT_ERROR";
                break;
            case AckType::ERROR_COMMAND_IGNORED:
                root["error_type"] = "COMMAND_IGNORED";
                break;
            case AckType::ERROR_INVALID_ARGUMENT:
                root["error_type"] = "INVALID_ARGUMENT";
                break;
            case AckType::ERROR_RUNTIME_ERROR:
                root["error_type"] = "RUNTIME_ERROR";
                break;
            case AckType::ERROR_TGAPI_EXCEPTION:
                root["error_type"] = "TGAPI_EXCEPTION";
                break;
            default:
                DLOG(WARNING)
                    << "Unknown ack type: " << static_cast<int>(ack.result);
                break;
        }
    }
    return nodeToPacket(Command::CMD_GENERIC_ACK, root, session_token);
}

struct WriteMsgToChatId {
    ChatId chat;          // destination chatid
    std::string message;  // Msg to send

    static std::optional<WriteMsgToChatId> fromBuffer(
        const std::uint8_t* buffer,
        TgBotSocket::Packet::Header::length_type size,
        TgBotSocket::PayloadType type) {
        WriteMsgToChatId result{};
        switch (type) {
            case PayloadType::Binary:
                if (size != sizeof(data::WriteMsgToChatId)) {
                    DLOG(WARNING)
                        << "Payload size mismatch on WriteMsgToChatId";
                    return std::nullopt;
                }
                {
                    const auto* data =
                        reinterpret_cast<const data::WriteMsgToChatId*>(buffer);
                    result.chat = data->chat;
                    result.message = safeParse(data->message);
                }
                return result;
            case PayloadType::Json: {
                auto _root = parseAndCheck(buffer, size, {"chat", "message"});
                if (!_root) {
                    return std::nullopt;
                }
                auto& root = _root.value();
                result.chat = root["chat"].as<ChatId>();
                result.message = root["message"].asString();
                return result;
            }
            default:
                LOG(ERROR) << "Invalid payload type for WriteMsgToChatId";
                return std::nullopt;
        }
    }
};

struct ObserveChatId {
    ChatId chat;
    bool observe;

    static std::optional<ObserveChatId> fromBuffer(
        const std::uint8_t* buffer,
        TgBotSocket::Packet::Header::length_type size,
        TgBotSocket::PayloadType type) {
        ObserveChatId result{};
        switch (type) {
            case PayloadType::Binary:
                if (size != sizeof(data::ObserveChatId)) {
                    DLOG(WARNING) << "Payload size mismatch on ObserveChatId";
                    return std::nullopt;
                }
                {
                    const auto* data =
                        reinterpret_cast<const data::ObserveChatId*>(buffer);
                    result.chat = data->chat;
                    result.observe = data->observe;
                }
                return result;
            case PayloadType::Json: {
                auto _root = parseAndCheck(buffer, size, {"chat", "observe"});
                if (!_root) {
                    return std::nullopt;
                }
                auto& root = _root.value();
                result.chat = root["chat"].as<ChatId>();
                result.observe = root["observe"].asBool();
                return result;
            }
            default:
                LOG(ERROR) << "Invalid payload type for ObserveChatId";
                return std::nullopt;
        }
    }
};

struct SendFileToChatId {
    ChatId chat;                           // Destination ChatId
    TgBotSocket::data::FileType fileType;  // File type for file
    std::filesystem::path filePath;        // Path to file (local)

    static std::optional<SendFileToChatId> fromBuffer(
        const std::uint8_t* buffer,
        TgBotSocket::Packet::Header::length_type size,
        TgBotSocket::PayloadType type) {
        SendFileToChatId result{};
        switch (type) {
            case PayloadType::Binary:
                if (size < sizeof(data::SendFileToChatId)) {
                    DLOG(WARNING)
                        << "Payload size mismatch on SendFileToChatId";
                    return std::nullopt;
                }
                {
                    const auto* data =
                        reinterpret_cast<const data::SendFileToChatId*>(buffer);
                    result.chat = data->chat;
                    result.fileType = data->fileType;
                    result.filePath = safeParse(data->filePath);
                }
                return result;
            case PayloadType::Json: {
                auto _root = parseAndCheck(buffer, size,
                                           {"chat", "fileType", "filePath"});
                if (!_root) {
                    return std::nullopt;
                }
                auto& root = _root.value();
                result.chat = root["chat"].as<ChatId>();
                result.fileType = static_cast<TgBotSocket::data::FileType>(
                    root["fileType"].asInt());
                result.filePath = root["filePath"].asString();
                return result;
            }
            default:
                LOG(ERROR) << "Invalid payload type for SendFileToChatId";
                return std::nullopt;
        }
    }
};

struct ObserveAllChats {
    bool observe;  // new state for all chats,
                   // true/false - Start/Stop observing

    static std::optional<ObserveAllChats> fromBuffer(
        const std::uint8_t* buffer,
        TgBotSocket::Packet::Header::length_type size,
        TgBotSocket::PayloadType type) {
        ObserveAllChats result{};
        switch (type) {
            case PayloadType::Binary:
                if (size != sizeof(data::ObserveAllChats)) {
                    DLOG(WARNING)
                        << "Payload size mismatch on ObserveAllChats for size: "
                        << size;
                    return std::nullopt;
                }
                {
                    const auto* data =
                        reinterpret_cast<const data::ObserveAllChats*>(buffer);
                    result.observe = data->observe;
                }
                return result;
            case PayloadType::Json: {
                auto _root = parseAndCheck(buffer, size, {"observe"});
                if (!_root) {
                    return std::nullopt;
                }
                auto& root = _root.value();
                result.observe = root["observe"].asBool();
                return result;
            }
            default:
                LOG(ERROR) << "Invalid payload type for ObserveAllChats";
                return std::nullopt;
        }
    }
};

std::optional<Packet::Header::length_type> findBorderOffset(
    const uint8_t* buffer, Packet::Header::length_type size) {
    Packet::Header::length_type offset = 0;
    for (Packet::Header::length_type i = 0; i < size; ++i) {
        if (buffer[i] == data::JSON_BYTE_BORDER) {
            LOG(INFO) << "Found JSON_BYTE_BORDER in offset " << i;
            return i;
        }
    }
    LOG(WARNING) << "JSON_BYTE_BORDER not found in buffer";
    return std::nullopt;
}

Packet GenericAckToPacket(
    const GenericAck& gn, const TgBotSocket::PayloadType payloadType,
    const TgBotSocket::Packet::Header::session_token_type& token) {
    LOG(INFO) << "Sending ack: " << std::boolalpha
              << (gn.result == AckType::SUCCESS);
    switch (payloadType) {
        case PayloadType::Binary: {
            return createPacket(Command::CMD_GENERIC_ACK, &gn,
                                sizeof(GenericAck),
                                TgBotSocket::PayloadType::Binary, token);
        } break;
        case PayloadType::Json: {
            return toJSONPacket(gn, token);
        } break;
        default:
            LOG(ERROR) << "Invalid payload type for GenericAck";
            return {};
    }
}

struct TransferFileMeta : SocketFile2DataHelper::Params {
    static std::optional<TransferFileMeta> fromBuffer(
        const uint8_t* buffer, TgBotSocket::Packet::Header::length_type size,
        TgBotSocket::PayloadType type) {
        TransferFileMeta result{};
        switch (type) {
            case PayloadType::Binary: {
                if (size < sizeof(data::FileTransferMeta)) {
                    DLOG(WARNING) << "Payload size mismatch on UploadFileMeta";
                    return std::nullopt;
                }
                {
                    const auto* data =
                        reinterpret_cast<const data::FileTransferMeta*>(buffer);
                    result.filepath = safeParse(data->srcfilepath);
                    result.destfilepath = safeParse(data->destfilepath);
                    result.options = data->options;
                    result.hash = data->sha256_hash;
                    result.file_size = size - sizeof(data::FileTransferMeta);
                    result.filebuffer = buffer + sizeof(data::FileTransferMeta);
                }
                return result;
            }
            case PayloadType::Json: {
                const auto offset =
                    findBorderOffset(buffer, size).value_or(size);
                std::string json(reinterpret_cast<const char*>(buffer), offset);
                auto _root = parseAndCheck(buffer, size,
                                           {"srcfilepath", "destfilepath"});
                if (!_root) {
                    return std::nullopt;
                }
                auto& root = _root.value();
                result.filepath = root["srcfilepath"].asString();
                result.destfilepath = root["destfilepath"].asString();
                data::FileTransferMeta::Options options;
                options.overwrite = root["options"]["overwrite"].asBool();
                options.hash_ignore = root["options"]["hash_ignore"].asBool();
                options.dry_run = root["options"]["dry_run"].asBool();
                if (!options.hash_ignore && !root.isMember("hash")) {
                    LOG(WARNING)
                        << "hash_ignore is false, but hash is not provided.";
                    return std::nullopt;
                }
                if (root.isMember("hash")) {
                    auto parsed = hexDecode<SHA256_DIGEST_LENGTH>(
                        root["hash"].asString());
                    if (!parsed) {
                        return std::nullopt;
                    }
                    result.hash = parsed.value();
                }
                result.options = options;
                result.file_size = size - offset;
                result.filebuffer = buffer + offset;
                return result;
            }
            default:
                LOG(ERROR) << "Invalid payload type for TransferFileMeta";
                return std::nullopt;
        }
    }
};

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

GenericAck SocketInterfaceTgBot::handle_CtrlSpamBlock(const std::uint8_t* ptr) {
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
           InputFile::fromFile(file, getMIMEString(resource, file)));
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
            Json::Value payload;
            payload["start_time"] = fmt::format("{}", startTp);
            payload["current_time"] = fmt::format("{}", now);
            payload["uptime"] = fmt::format("{:%Hh %Mm %Ss}", diff);
            LOG(INFO) << "Sending JSON back: " << payload.toStyledString();
            return nodeToPacket(Command::CMD_GET_UPTIME_CALLBACK, payload,
                                token);
        } break;
        default:
            LOG(WARNING) << "Unsupported payload type: "
                         << static_cast<int>(type);
    }
    return std::nullopt;
}

template <typename DataT>
bool CHECK_PACKET_SIZE(Packet& pkt) {
    if (pkt.header.data_size != sizeof(DataT)) {
        LOG(WARNING) << fmt::format(
            "Invalid packet size for cmd: {}. Got {}, diff: {}", pkt.header.cmd,
            pkt.header.data_size,
            -(int64_t)(sizeof(DataT) - pkt.header.data_size));
        return false;
    }
    return true;
}

bool SocketInterfaceTgBot::verifyHeader(const Packet& packet) {
    decltype(session_table)::iterator iter;

    for (iter = session_table.begin(); iter != session_table.end(); ++iter) {
        if (std::ranges::equal(iter->first, packet.header.session_token)) {
            break;
        }
    }
    if (iter == session_table.end()) {
        LOG(WARNING) << "Received packet with unknown session token";
        return false;
    }
    if (std::chrono::system_clock::now() > iter->second.expiry) {
        LOG(WARNING) << "Session token expired, rejecting and removing";
        session_table.erase(iter);
        return false;
    }
    if (iter->second.last_nonce >= packet.header.nonce) {
        LOG(WARNING) << "Received packet with outdated nonce, ignore";
        return false;
    }
    iter->second.last_nonce = packet.header.nonce;
    return true;
}

void SocketInterfaceTgBot::handle_OpenSession(const TgBotSocket::Context& ctx) {
    auto key = StringTools::generateRandomString(
        TgBotSocket::Crypto::SESSION_TOKEN_LENGTH);
    Packet::Header::nounce_type last_nounce{};
    auto tp = std::chrono::system_clock::now() + std::chrono::hours(1);

    LOG(INFO) << "Created new session with key: " << std::quoted(key);
    session_table.emplace(key, Session(key, last_nounce, tp));

    Json::Value response;
    response["session_token"] = key;
    response["expiration_time"] = fmt::format("{:%Y-%m-%d %H:%M:%S}", tp);

    Packet::Header::session_token_type session_token;
    std::ranges::copy_n(key.begin(), Crypto::SESSION_TOKEN_LENGTH,
                        session_token.begin());

    ctx.write(
        nodeToPacket(Command::CMD_OPEN_SESSION_ACK, response, session_token));
}

void SocketInterfaceTgBot::handle_CloseSession(
    const TgBotSocket::Packet::Header::session_token_type& token) {
    decltype(session_table)::iterator iter;
    for (iter = session_table.begin(); iter != session_table.end(); ++iter) {
        if (std::ranges::equal(iter->first, token)) {
            break;
        }
    }
    if (iter == session_table.end()) {
        LOG(WARNING) << "Received packet with unknown session token";
        return;
    }
    session_table.erase(iter);
    LOG(INFO) << "Session with key " << std::string(token.data(), token.size())
              << " closed";
}

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
