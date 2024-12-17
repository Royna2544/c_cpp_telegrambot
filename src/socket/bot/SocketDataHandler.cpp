#include <fmt/chrono.h>
#include <json/json.h>
#include <tgbot/TgException.h>
#include <tgbot/tools/StringTools.h>
#include <trivial_helpers/_std_chrono_templates.h>

#include <ManagedThreads.hpp>
#include <ResourceManager.hpp>
#include <TgBotSocket_Export.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <global_handlers/ChatObserver.hpp>
#include <global_handlers/SpamBlock.hpp>
#include <initializer_list>
#include <mutex>
#include <socket/TgBotCommandMap.hpp>
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
    static std::once_flag once;
    static Json::Value doc;
    std::string extension = fs::path(path).extension().string();

    std::call_once(once, [resource] {
        std::string_view buf;
        buf = resource->get("mimeData.json");
        Json::Reader reader;
        if (!reader.parse(buf.data(), doc)) {
            LOG(ERROR) << "Failed to parse mimedata: "
                       << reader.getFormattedErrorMessages();
        }
    });
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

std::optional<Json::Value> parseAndCheck(
    const void* buf, TgBotSocket::Packet::Header::length_type length,
    const std::initializer_list<const char*> nodes) {
    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(std::string(static_cast<const char*>(buf), length),
                      root)) {
        LOG(WARNING) << "Failed to parse json: "
                     << reader.getFormattedErrorMessages();
        return std::nullopt;
    }
    if (!root.isObject()) {
        LOG(WARNING) << "Expected an object in json";
        return std::nullopt;
    }
    for (const auto& node : nodes) {
        if (!root.isMember(node)) {
            LOG(WARNING) << fmt::format("Missing node '{}' in json", node);
            return std::nullopt;
        }
    }
    return root;
}

Packet nodeToPacket(const Command& command, const Json::Value& json,
                    const Packet::Header::session_token_type& session_token) {
    std::string result;
    Json::FastWriter writer;
    result = writer.write(json);
    auto packet = createPacket(command, result.c_str(), result.size(),
                               PayloadType::Json, session_token);
    return packet;
}

Packet toJSONPacket(const GenericAck& ack,
                    const Packet::Header::session_token_type& session_token) {
    Json::Value root;
    root["result"] = ack.result == AckType::SUCCESS;
    if (ack.result != AckType::SUCCESS) {
        root["error_msg"] = ack.error_msg.data();
        switch (ack.result) {
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

template <size_t N>
std::string safeParse(const std::array<char, N>& buf) {
    // Create a larger array to hold the null-terminated string
    std::array<char, N + 1> safebuf{};

    // Safely copy N characters from buf to safebuf
    std::ranges::copy_n(buf.begin(), N, safebuf.begin());

    // Null-terminate the new buffer
    safebuf[N] = '\0';

    return safebuf.data();
}

struct WriteMsgToChatId {
    ChatId chat;          // destination chatid
    std::string message;  // Msg to send

    static std::optional<WriteMsgToChatId> fromBuffer(
        const void* buffer, TgBotSocket::Packet::Header::length_type size,
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
                        static_cast<const data::WriteMsgToChatId*>(buffer);
                    result.chat = data->chat;
                    result.message = safeParse(data->message);
                }
                return result;
            case PayloadType::Json: {
                auto _root = parseAndCheck(buffer, size, {"chat", "message"});
                if (!_root) {
                    return std::nullopt;
                }
                auto root = _root.value();
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
        const void* buffer, TgBotSocket::Packet::Header::length_type size,
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
                        static_cast<const data::ObserveChatId*>(buffer);
                    result.chat = data->chat;
                    result.observe = data->observe;
                }
                return result;
            case PayloadType::Json: {
                auto _root = parseAndCheck(buffer, size, {"chat", "observe"});
                if (!_root) {
                    return std::nullopt;
                }
                auto root = _root.value();
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
        const void* buffer, TgBotSocket::Packet::Header::length_type size,
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
                        static_cast<const data::SendFileToChatId*>(buffer);
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
                auto root = _root.value();
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
        const void* buffer, TgBotSocket::Packet::Header::length_type size,
        TgBotSocket::PayloadType type) {
        ObserveAllChats result{};
        switch (type) {
            case PayloadType::Binary:
                if (size != sizeof(data::ObserveAllChats)) {
                    DLOG(WARNING) << "Payload size mismatch on ObserveAllChats";
                    return std::nullopt;
                }
                {
                    const auto* data =
                        static_cast<const data::ObserveAllChats*>(buffer);
                    result.observe = data->observe;
                }
                return result;
            case PayloadType::Json: {
                auto _root = parseAndCheck(buffer, size, {"observe"});
                if (!_root) {
                    return std::nullopt;
                }
                auto root = _root.value();
                result.observe = root["observe"].asBool();
                return result;
            }
            default:
                LOG(ERROR) << "Invalid payload type for ObserveAllChats";
                return std::nullopt;
        }
    }
};

GenericAck SocketInterfaceTgBot::handle_WriteMsgToChatId(
    const void* ptr, TgBotSocket::Packet::Header::length_type len,
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

GenericAck SocketInterfaceTgBot::handle_CtrlSpamBlock(const void* ptr) {
    const auto* data =
        static_cast<const TgBotSocket::data::CtrlSpamBlock*>(ptr);
    spamblock->setConfig(*data);
    return GenericAck::ok();
}

GenericAck SocketInterfaceTgBot::handle_ObserveChatId(
    const void* ptr, TgBotSocket::Packet::Header::length_type len,
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
    const void* ptr, TgBotSocket::Packet::Header::length_type len,
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
    const void* ptr, TgBotSocket::Packet::Header::length_type len,
    TgBotSocket::PayloadType type) {
    auto data = ObserveAllChats::fromBuffer(ptr, len, type);
    observer->observeAll(static_cast<const ObserveAllChats*>(ptr)->observe);
    return GenericAck::ok();
}

GenericAck SocketInterfaceTgBot::handle_UploadFile(
    const void* ptr, TgBotSocket::Packet::Header::length_type len) {
    if (!helper->DataToFile<SocketFile2DataHelper::Pass::UPLOAD_FILE>(ptr,
                                                                      len)) {
        return GenericAck(AckType::ERROR_RUNTIME_ERROR, "Failed to write file");
    }
    return GenericAck::ok();
}

UploadFileDryCallback SocketInterfaceTgBot::handle_UploadFileDry(
    const void* ptr, TgBotSocket::Packet::Header::length_type len) {
    bool ret = false;
    const auto* f = static_cast<const data::UploadFileMeta*>(ptr);
    UploadFileDryCallback callback;
    callback.requestdata = *f;

    ret = helper->DataToFile<SocketFile2DataHelper::Pass::UPLOAD_FILE_DRY>(ptr,
                                                                           len);
    if (!ret) {
        copyTo(callback.error_msg, "Options verification failed");
        callback.result = AckType::ERROR_COMMAND_IGNORED;
    } else {
        copyTo(callback.error_msg, "OK");
        callback.result = AckType::SUCCESS;
    }
    return callback;
}

bool SocketInterfaceTgBot::handle_DownloadFile(
    const TgBotSocket::Context& ctx, const void* ptr,
    const TgBotSocket::Packet::Header::session_token_type& token) {
    const auto* data = static_cast<const data::DownloadFile*>(ptr);
    SocketFile2DataHelper::DataFromFileParam params;
    params.filepath = data->filepath.data();
    params.destfilepath = data->destfilename.data();
    auto pkt = helper->DataFromFile<SocketFile2DataHelper::Pass::DOWNLOAD_FILE>(
        params, token);
    if (!pkt) {
        LOG(ERROR) << "Failed to prepare download file packet";
        return false;
    }
    ctx.write(pkt.value());
    return true;
}

bool SocketInterfaceTgBot::handle_GetUptime(
    const TgBotSocket::Context& ctx,
    const TgBotSocket::Packet::Header::session_token_type& token) {
    auto now = std::chrono::system_clock::now();
    const auto diff = to_secs(now - startTp);
    GetUptimeCallback callback{};

    copyTo(callback.uptime, fmt::format("Uptime: {:%H:%M:%S}", diff).c_str());
    LOG(INFO) << "Sending text back: " << std::quoted(callback.uptime.data());
    ctx.write(createPacket(Command::CMD_GET_UPTIME_CALLBACK, &callback,
                           sizeof(callback), PayloadType::Binary, token));
    return true;
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
    std::string_view their_token(packet.header.session_token.data(),
                                 Packet::Header::SESSION_TOKEN_LENGTH);
    if (their_token.empty()) {
        LOG(WARNING) << "Received packet with empty session token";
        return false;
    }
    if (!session_table.contains(their_token.data())) {
        LOG(WARNING) << fmt::format(
            "Received packet with unknown session token: {}", their_token);
        return false;
    }
    auto& session = session_table[their_token.data()];
    if (std::chrono::system_clock::now() > session.expiry) {
        LOG(WARNING) << fmt::format("Session token expired: {}", their_token);
        session_table.erase(their_token.data());
        return false;
    }
    if (session.last_nonce >= packet.header.nonce) {
        LOG(WARNING) << "Received packet with outdated nonce, ignore";
        return false;
    }
    session.last_nonce = packet.header.nonce;
    return true;
}

void SocketInterfaceTgBot::handle_OpenSession(const TgBotSocket::Context& ctx) {
    auto key = StringTools::generateRandomString(
        TgBotSocket::Packet::Header::SESSION_TOKEN_LENGTH);
    Packet::Header::nounce_type last_nounce{};
    auto tp = std::chrono::system_clock::now() + std::chrono::hours(1);

    LOG(INFO) << "Created new session with key: " << std::quoted(key);
    session_table.emplace(key, Session(key, last_nounce, tp));

    Json::Value response;
    response["session_token"] = key;
    response["expiration_time"] = fmt::format("{:%Y-%m-%d %H:%M:%S}", tp);

    Packet::Header::session_token_type session_token;
    std::ranges::copy(key, session_token.begin());

    ctx.write(
        nodeToPacket(Command::CMD_OPEN_SESSION_ACK, response, session_token));
}

void SocketInterfaceTgBot::handle_CloseSession(
    const TgBotSocket::Packet::Header::session_token_type& token) {
    auto it = session_table.find(std::string(token.data(), token.size()));
    if (it != session_table.end()) {
        session_table.erase(it);
        LOG(INFO) << "Session with key " << token.data() << " closed";
    } else {
        LOG(WARNING)
            << "Received close session request for unknown session token: "
            << token.data();
    }
}

void SocketInterfaceTgBot::handlePacket(const TgBotSocket::Context& ctx,
                                        TgBotSocket::Packet pkt) {
    const void* ptr = pkt.data.get();
    std::variant<UploadFileDryCallback, GenericAck, bool> ret;
    const auto invalidPacketAck =
        GenericAck(AckType::ERROR_COMMAND_IGNORED, "Invalid packet size");

    switch (pkt.header.cmd) {
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
            ret = handle_GetUptime(ctx, pkt.header.session_token);
            break;
        case Command::CMD_UPLOAD_FILE:
            ret = handle_UploadFile(ptr, pkt.header.data_size);
            break;
        case Command::CMD_UPLOAD_FILE_DRY:
            if (CHECK_PACKET_SIZE<data::UploadFileMeta>(pkt)) {
                ret = handle_UploadFileDry(ptr, pkt.header.data_size);
            } else {
                ret = UploadFileDryCallback(invalidPacketAck);
            }
            break;
        case Command::CMD_DOWNLOAD_FILE:
            ret = handle_DownloadFile(ctx, ptr, pkt.header.session_token);
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
    switch (pkt.header.cmd) {
        case Command::CMD_GET_UPTIME:
        case Command::CMD_DOWNLOAD_FILE: {
            // This has its own callback, so we don't need to send ack.
            bool result = std::get<bool>(ret);
            LOG_IF(WARNING, (!result))
                << fmt::format("Command failed: {}", pkt.header.cmd);
            break;
        }
        case Command::CMD_UPLOAD_FILE_DRY: {
            const auto result = std::get<UploadFileDryCallback>(ret);
            auto ackpkt =
                createPacket(Command::CMD_UPLOAD_FILE_DRY_CALLBACK, &result,
                             sizeof(UploadFileDryCallback), PayloadType::Binary,
                             pkt.header.session_token);
            LOG(INFO) << "Sending CMD_UPLOAD_FILE_DRY ack: " << std::boolalpha
                      << (result.result == AckType::SUCCESS);
            ctx.write(ackpkt);
            break;
        }
        case Command::CMD_WRITE_MSG_TO_CHAT_ID:
        case Command::CMD_CTRL_SPAMBLOCK:
        case Command::CMD_OBSERVE_CHAT_ID:
        case Command::CMD_SEND_FILE_TO_CHAT_ID:
        case Command::CMD_OBSERVE_ALL_CHATS:
        case Command::CMD_UPLOAD_FILE: {
            GenericAck result = std::get<GenericAck>(ret);
            LOG(INFO) << "Sending ack: " << std::boolalpha
                      << (result.result == AckType::SUCCESS);
            switch (pkt.header.data_type) {
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
