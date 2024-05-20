#include <BotReplyMessage.h>
#include <ChatObserver.h>
#include <ResourceManager.h>
#include <SingleThreadCtrl.h>
#include <SpamBlock.h>
#include <internal/_std_chrono_templates.h>
#include <random/RandomNumberGenerator.h>
#include <rapidjson/document.h>
#include <rapidjson/rapidjson.h>
#include <tgbot/types/InputFile.h>

#include <DatabaseBot.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <impl/bot/TgBotSocketFileHelper.hpp>
#include <impl/bot/TgBotSocketInterface.hpp>
#include <mutex>
#include <optional>
#include <variant>

// Come last
#include <socket/TgBotSocket.h>

using TgBot::Api;
using TgBot::InputFile;
namespace fs = std::filesystem;

namespace {

std::string getMIMEString(const std::string& path) {
    static std::once_flag once;
    static rapidjson::Document doc;
    std::string extension = fs::path(path).extension().string();

    std::call_once(once, [] {
        std::string_view buf;
        buf = ResourceManager::getInstance()->getResource("mimeData.json");
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
        bot_sendMessage(_bot, data->to, data->msg);
    } catch (const TgBot::TgException& e) {
        LOG(ERROR) << "Exception at handler: " << e.what();
        return GenericAck(AckType::ERROR_TGAPI_EXCEPTION, e.what());
    }
    return GenericAck();
}

GenericAck SocketInterfaceTgBot::handle_CtrlSpamBlock(const void* ptr) {
    const auto* data = static_cast<const CtrlSpamBlock*>(ptr);
    gSpamBlockCfg = *data;
    return GenericAck();
}

GenericAck SocketInterfaceTgBot::handle_ObserveChatId(const void* ptr) {
    const auto* data = static_cast<const ObserveChatId*>(ptr);
    auto obs = ChatObserver::getInstance();
    const std::lock_guard<std::mutex> _(obs->m);
    auto it = std::ranges::find(obs->observedChatIds, data->id);

    bool observe = data->observe;
    if (obs->observeAllChats) {
        return GenericAck(AckType::ERROR_COMMAND_IGNORED,
                          "CMD_OBSERVE_ALL_CHATS active");
    }
    if (it == obs->observedChatIds.end()) {
        if (observe) {
            obs->observedChatIds.push_back(data->id);
        } else {
            return GenericAck(AckType::ERROR_COMMAND_IGNORED,
                              "Target chat wasn't being observed");
        }
    } else {
        if (observe) {
            return GenericAck(AckType::ERROR_COMMAND_IGNORED,
                              "Target chat was already being observed");
        } else {
            LOG(INFO) << "Removing chat from observer";
            obs->observedChatIds.erase(it);
        }
    }
    return GenericAck();
}

GenericAck SocketInterfaceTgBot::handle_SendFileToChatId(const void* ptr) {
    const auto* data = static_cast<const SendFileToChatId*>(ptr);
    const auto* file = data->filepath;
    using FileOrId_t = boost::variant<InputFile::Ptr, std::string>;
    std::function<Message::Ptr(const Api&, ChatId, FileOrId_t)> fn;
    try {
        switch (data->type) {
            case TYPE_PHOTO:
                fn = [](const Api& api, ChatId id, const FileOrId_t file) {
                    return api.sendPhoto(id, file);
                };
                break;
            case TYPE_VIDEO:
                fn = [](const Api& api, ChatId id, const FileOrId_t file) {
                    return api.sendVideo(id, file);
                };
                break;
            case TYPE_GIF:
                fn = [](const Api& api, ChatId id, const FileOrId_t file) {
                    return api.sendAnimation(id, file);
                };
            case TYPE_DOCUMENT:
                fn = [](const Api& api, ChatId id, const FileOrId_t file) {
                    return api.sendDocument(id, file);
                };
                break;
            case TYPE_DICE: {
                static const std::vector<std::string> dices = {
                    "🎲", "🎯", "🏀", "⚽", "🎳", "🎰"};
                // TODO: More clean code?
                _bot.getApi().sendDice(
                    data->id, false, 0, nullptr,
                    dices[genRandomNumber(0, dices.size() - 1)]);
                return GenericAck();
            }
            default:
                fn = [](const Api&, ChatId, const FileOrId_t) {
                    return nullptr;
                };
                break;
        }
        // Try to send as local file first
        try {
            fn(_bot.getApi(), data->id,
               InputFile::fromFile(file, getMIMEString(file)));
        } catch (std::ifstream::failure& e) {
            LOG(INFO) << "Failed to send '" << file
                      << "' as local file, trying as Telegram "
                         "file id";
            fn(_bot.getApi(), data->id, std::string(file));
        }
    } catch (const TgBot::TgException& e) {
        LOG(ERROR) << "Exception at handler, " << e.what();
        return GenericAck(AckType::ERROR_TGAPI_EXCEPTION, e.what());
    }
    return GenericAck();
}

GenericAck SocketInterfaceTgBot::handle_ObserveAllChats(const void* ptr) {
    auto obs = ChatObserver::getInstance();
    const std::lock_guard<std::mutex> _(obs->m);
    obs->observeAllChats = *static_cast<const ObserveAllChats*>(ptr);
    return GenericAck();
}

GenericAck SocketInterfaceTgBot::handle_DeleteControllerById(const void* ptr) {
    DeleteControllerById data = *static_cast<const DeleteControllerById*>(ptr);
    enum SingleThreadCtrlManager::ThreadUsage threadUsage{};
    if (data < 0 || data >= SingleThreadCtrlManager::USAGE_MAX) {
        LOG(ERROR) << "Invalid controller id: " << data;
        return GenericAck(AckType::ERROR_INVALID_ARGUMENT,
                          "Invalid controller id");
    }
    threadUsage = static_cast<SingleThreadCtrlManager::ThreadUsage>(data);
    SingleThreadCtrlManager::getInstance()->destroyController(threadUsage);
    return GenericAck();
}

GenericAck SocketInterfaceTgBot::handle_UploadFile(
    const void* ptr, TgBotCommandPacketHeader::length_type len) {
    if (fileData_tofile(ptr, len)) {
        return GenericAck();
    }
    return GenericAck(AckType::ERROR_RUNTIME_ERROR, "Failed to write file");
}

bool SocketInterfaceTgBot::handle_DownloadFile(SocketConnContext ctx,
                                               const void* ptr) {
    const auto* data = static_cast<const DownloadFile*>(ptr);
    auto pkt = fileData_fromFile(CMD_DOWNLOAD_FILE_CALLBACK, data->filepath,
                                 data->destfilename);
    if (!pkt) {
        LOG(ERROR) << "Failed to prepare download file packet";
        return false;
    }
    interface->writeToSocket(ctx, pkt->toSocketData());
    return true;
}

bool SocketInterfaceTgBot::handle_GetUptime(SocketConnContext ctx,
                                            const void* /*ptr*/) {
    auto now = std::chrono::system_clock::now();
    const auto diff = now - startTp;
    std::stringstream uptime;
    std::string uptimeStr;

    uptime << "Uptime: " << to_string(diff);
    uptimeStr = uptime.str();
    LOG(INFO) << "Sending text back: " << std::quoted(uptimeStr);
    TgBotCommandPacket pkt(CMD_GET_UPTIME_CALLBACK, uptimeStr.c_str(),
                           sizeof(GetUptimeCallback) - 1);
    interface->writeToSocket(ctx, pkt.toSocketData());
    return true;
}

void SocketInterfaceTgBot::handle_CommandPacket(SocketConnContext ctx,
                                                TgBotCommandPacket pkt) {
    const void* ptr = pkt.data.get();
    using namespace TgBotCommandData;
    std::variant<GenericAck, bool> ret;

    switch (pkt.header.cmd) {
        case CMD_WRITE_MSG_TO_CHAT_ID:
            ret = handle_WriteMsgToChatId(ptr);
            break;
        case CMD_CTRL_SPAMBLOCK:
            ret = handle_CtrlSpamBlock(ptr);
            break;
        case CMD_OBSERVE_CHAT_ID:
            ret = handle_ObserveChatId(ptr);
            break;
        case CMD_SEND_FILE_TO_CHAT_ID:
            ret = handle_SendFileToChatId(ptr);
            break;
        case CMD_OBSERVE_ALL_CHATS:
            ret = handle_ObserveAllChats(ptr);
            break;
        case CMD_DELETE_CONTROLLER_BY_ID:
            ret = handle_DeleteControllerById(ptr);
            break;
        case CMD_GET_UPTIME:
            ret = handle_GetUptime(ctx, ptr);
            break;
        case CMD_UPLOAD_FILE:
            ret = handle_UploadFile(ptr, pkt.header.data_size);
            break;
        case CMD_DOWNLOAD_FILE:
            ret = handle_DownloadFile(ctx, ptr);
            break;
        default:
            if (TgBotCmd::isClientCommand(pkt.header.cmd)) {
                LOG(ERROR) << "Unhandled cmd: "
                           << TgBotCmd::toStr(pkt.header.cmd);
            } else {
                LOG(WARNING) << "cmd ignored (as internal): "
                             << static_cast<int>(pkt.header.cmd);
            }
            return;
    };
    switch (pkt.header.cmd) {
        case CMD_GET_UPTIME:
        case CMD_DOWNLOAD_FILE: {
            // This has its own callback, so we don't need to send ack.
            bool result = std::get<bool>(ret);
            DLOG_IF(INFO, (!result))
                << "Command failed: " << TgBotCmd::toStr(pkt.header.cmd);
            break;
        }
        case CMD_WRITE_MSG_TO_CHAT_ID:
        case CMD_CTRL_SPAMBLOCK:
        case CMD_OBSERVE_CHAT_ID:
        case CMD_SEND_FILE_TO_CHAT_ID:
        case CMD_OBSERVE_ALL_CHATS:
        case CMD_DELETE_CONTROLLER_BY_ID:
        case CMD_UPLOAD_FILE: {
            GenericAck result = std::get<GenericAck>(ret);
            TgBotCommandPacket ackpkt(CMD_GENERIC_ACK, &result,
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
