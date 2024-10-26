#include <ResourceManager.h>
#include <fmt/chrono.h>
#include <rapidjson/document.h>
#include <rapidjson/rapidjson.h>
#include <tgbot/TgException.h>
#include <trivial_helpers/_std_chrono_templates.h>

#include <ManagedThreads.hpp>
#include <TgBotSocket_Export.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <global_handlers/ChatObserver.hpp>
#include <global_handlers/SpamBlock.hpp>
#include <impl/bot/TgBotSocketFileHelperNew.hpp>
#include <impl/bot/TgBotSocketInterface.hpp>
#include <mutex>
#include <socket/TgBotCommandMap.hpp>
#include <utility>
#include <variant>

using TgBot::InputFile;
namespace fs = std::filesystem;
using namespace TgBotSocket;
using namespace TgBotSocket::callback;
using namespace TgBotSocket::data;

namespace {

std::string getMIMEString(const ResourceManager* resource, const std::string& path) {
    static std::once_flag once;
    static rapidjson::Document doc;
    std::string extension = fs::path(path).extension().string();

    std::call_once(once, [resource] {
        std::string_view buf;
        buf = resource->getResource("mimeData.json");
        doc.Parse(buf.data());
        // This should be an assert, we know the data file at compile time
        LOG_IF(FATAL, doc.HasParseError())
            << "Failed to parse mimedata: " << doc.GetParseError();
    });
    if (!extension.empty()) {
        for (rapidjson::SizeType i = 0; i < doc.Size(); i++) {
            const rapidjson::Value& oneJsonElement = doc[i];
            const rapidjson::Value& availableTypes =
                oneJsonElement["types"].GetArray();
            for (rapidjson::SizeType i = 0; i < availableTypes.Size(); i++) {
                if (availableTypes[i].GetString() == extension) {
                    const auto* mime = oneJsonElement["name"].GetString();
                    LOG(INFO) << "Found MIME type: '" << mime << "'";
                    return mime;
                }
            }
        }
        LOG(WARNING) << "Unknown file extension: '" << extension << "'";
    }
    return "application/octet-stream";
}
}  // namespace

GenericAck SocketInterfaceTgBot::handle_WriteMsgToChatId(const void* ptr) {
    const auto* data = static_cast<const WriteMsgToChatId*>(ptr);
    try {
        api->sendMessage(data->chat, data->message);
    } catch (const TgBot::TgException& e) {
        LOG(ERROR) << "Exception at handler: " << e.what();
        return GenericAck(AckType::ERROR_TGAPI_EXCEPTION, e.what());
    }
    return GenericAck::ok();
}

GenericAck SocketInterfaceTgBot::handle_CtrlSpamBlock(const void* ptr) {
    const auto* data = static_cast<const CtrlSpamBlock*>(ptr);
    spamblock->spamBlockConfig = *data;
    return GenericAck::ok();
}

GenericAck SocketInterfaceTgBot::handle_ObserveChatId(const void* ptr) {
    const auto* data = static_cast<const ObserveChatId*>(ptr);

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

GenericAck SocketInterfaceTgBot::handle_SendFileToChatId(const void* ptr) {
    const auto* data = static_cast<const SendFileToChatId*>(ptr);
    const auto* file = data->filePath.data();
    std::function<Message::Ptr(ChatId, const TgBotApi::FileOrMedia&)> fn;
    switch (data->fileType) {
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
        fn(data->chat, InputFile::fromFile(file, getMIMEString(resource, file)));
    } catch (std::ifstream::failure& e) {
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

GenericAck SocketInterfaceTgBot::handle_ObserveAllChats(const void* ptr) {
    observer->observeAll(static_cast<const ObserveAllChats*>(ptr)->observe);
    return GenericAck::ok();
}

GenericAck SocketInterfaceTgBot::handle_DeleteControllerById(const void* ptr) {
    return GenericAck{AckType::ERROR_COMMAND_IGNORED, "This command is removed."};
}

GenericAck SocketInterfaceTgBot::handle_UploadFile(
    const void* ptr, TgBotSocket::PacketHeader::length_type len) {
    if (!helper->DataToFile<SocketFile2DataHelper::Pass::UPLOAD_FILE>(ptr,
                                                                      len)) {
        return GenericAck(AckType::ERROR_RUNTIME_ERROR, "Failed to write file");
    }
    return GenericAck::ok();
}

UploadFileDryCallback SocketInterfaceTgBot::handle_UploadFileDry(
    const void* ptr, TgBotSocket::PacketHeader::length_type len) {
    bool ret = false;
    const auto* f = static_cast<const UploadFileDry*>(ptr);
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

bool SocketInterfaceTgBot::handle_DownloadFile(SocketConnContext ctx,
                                               const void* ptr) {
    const auto* data = static_cast<const DownloadFile*>(ptr);
    SocketFile2DataHelper::DataFromFileParam params;
    params.filepath = data->filepath.data();
    params.destfilepath = data->destfilename.data();
    auto pkt = helper->DataFromFile<SocketFile2DataHelper::Pass::DOWNLOAD_FILE>(
        params);
    if (!pkt) {
        LOG(ERROR) << "Failed to prepare download file packet";
        return false;
    }
    interface->writeToSocket(std::move(ctx), pkt->toSocketData());
    return true;
}

bool SocketInterfaceTgBot::handle_GetUptime(SocketConnContext ctx,
                                            const void* /*ptr*/) {
    auto now = std::chrono::system_clock::now();
    const auto diff = now - startTp;
    GetUptimeCallback callback{};

    (void)std::snprintf(callback.uptime.data(), callback.uptime.size() - 1,
                        "%s", fmt::format("Uptime: {:%H:%M:%S}", diff).c_str());
    LOG(INFO) << "Sending text back: " << std::quoted(callback.uptime.data());
    Packet pkt(Command::CMD_GET_UPTIME_CALLBACK, callback);
    interface->writeToSocket(std::move(ctx), pkt.toSocketData());
    return true;
}

template <typename DataT>
bool CHECK_PACKET_SIZE(Packet& pkt) {
    if (pkt.header.data_size != sizeof(DataT)) {
        LOG(WARNING) << "Invalid packet size for cmd: "
                     << CommandHelpers::toStr(pkt.header.cmd)
                     << " diff: " << pkt.header.data_size - sizeof(DataT);
        return false;
    }
    return true;
}

#define HANDLE(type, args...)               \
    ([&]() -> decltype(ret) {               \
        decltype(ret) __ret;                \
        if (CHECK_PACKET_SIZE<type>(pkt)) { \
            __ret = handle_##type(ptr);     \
        } else {                            \
            __ret = invalidPacketAck;       \
        }                                   \
        return __ret;                       \
    }())

#define HANDLE_EXT(type)                                      \
    ([&]() -> decltype(ret) {                                 \
        decltype(ret) __ret;                                  \
        if (CHECK_PACKET_SIZE<type>(pkt)) {                   \
            __ret = handle_##type(ptr, pkt.header.data_size); \
        } else {                                              \
            __ret = invalidPacketAck;                         \
        }                                                     \
        return __ret;                                         \
    }())

void SocketInterfaceTgBot::handlePacket(SocketConnContext ctx,
                                        TgBotSocket::Packet pkt) {
    const void* ptr = pkt.data.get();
    std::variant<GenericAck, UploadFileDryCallback, bool> ret;
    const auto invalidPacketAck =
        GenericAck(AckType::ERROR_COMMAND_IGNORED, "Invalid packet size");

    switch (pkt.header.cmd) {
        case Command::CMD_WRITE_MSG_TO_CHAT_ID:
            ret = HANDLE(WriteMsgToChatId, 1);
            break;
        case Command::CMD_CTRL_SPAMBLOCK:
            ret = HANDLE(CtrlSpamBlock);
            break;
        case Command::CMD_OBSERVE_CHAT_ID:
            ret = HANDLE(ObserveChatId);
            break;
        case Command::CMD_SEND_FILE_TO_CHAT_ID:
            ret = HANDLE(SendFileToChatId);
            break;
        case Command::CMD_OBSERVE_ALL_CHATS:
            ret = HANDLE(ObserveAllChats);
            break;
        case Command::CMD_DELETE_CONTROLLER_BY_ID:
            ret = HANDLE(DeleteControllerById);
            break;
        case Command::CMD_GET_UPTIME:
            ret = handle_GetUptime(ctx, ptr);
            break;
        case Command::CMD_UPLOAD_FILE:
            ret = handle_UploadFile(ptr, pkt.header.data_size);
            break;
        case Command::CMD_UPLOAD_FILE_DRY:
            ret = HANDLE_EXT(UploadFileDry);
            break;
        case Command::CMD_DOWNLOAD_FILE:
            ret = handle_DownloadFile(ctx, ptr);
            break;
        default:
            if (CommandHelpers::isClientCommand(pkt.header.cmd)) {
                LOG(ERROR) << "Unhandled cmd: "
                           << CommandHelpers::toStr(pkt.header.cmd);
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
            DLOG_IF(INFO, (!result))
                << "Command failed: " << CommandHelpers::toStr(pkt.header.cmd);
            break;
        }
        case Command::CMD_UPLOAD_FILE_DRY: {
            const auto result = std::get<UploadFileDryCallback>(ret);
            Packet ackpkt(Command::CMD_UPLOAD_FILE_DRY_CALLBACK, &result,
                          sizeof(UploadFileDryCallback));
            LOG(INFO) << "Sending CMD_UPLOAD_FILE_DRY ack: " << std::boolalpha
                      << (result.result == AckType::SUCCESS);
            interface->writeToSocket(ctx, ackpkt.toSocketData());
            break;
        }
        case Command::CMD_WRITE_MSG_TO_CHAT_ID:
        case Command::CMD_CTRL_SPAMBLOCK:
        case Command::CMD_OBSERVE_CHAT_ID:
        case Command::CMD_SEND_FILE_TO_CHAT_ID:
        case Command::CMD_OBSERVE_ALL_CHATS:
        case Command::CMD_DELETE_CONTROLLER_BY_ID:
        case Command::CMD_UPLOAD_FILE: {
            GenericAck result = std::get<GenericAck>(ret);
            Packet ackpkt(Command::CMD_GENERIC_ACK, &result,
                          sizeof(GenericAck));
            LOG(INFO) << "Sending ack: " << std::boolalpha
                      << (result.result == AckType::SUCCESS);
            interface->writeToSocket(ctx, ackpkt.toSocketData());
            break;
        }
        default:
            LOG(ERROR) << "Unknown command: "
                       << static_cast<int>(pkt.header.cmd);
            break;
    }
}
