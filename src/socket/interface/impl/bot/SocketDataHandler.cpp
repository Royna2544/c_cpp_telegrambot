#include <BotReplyMessage.h>
#include <ChatObserver.h>
#include <ResourceManager.h>
#include <SingleThreadCtrl.h>
#include <SpamBlock.h>
#include <internal/_std_chrono_templates.h>
#include <random/RandomNumberGenerator.h>
#include <rapidjson/document.h>
#include <rapidjson/rapidjson.h>
#include <socket/TgBotSocket.h>
#include <tgbot/types/InputFile.h>

#include <DatabaseBot.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>

#include "SocketBase.hpp"
#include "SocketDescriptor_defs.hpp"
#include "TgBotSocketInterface.hpp"

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

bool SocketInterfaceTgBot::handle_WriteMsgToChatId(const void* ptr) {
    const auto* data = reinterpret_cast<const WriteMsgToChatId*>(ptr);
    try {
        bot_sendMessage(_bot, data->to, data->msg);
    } catch (const TgBot::TgException& e) {
        LOG(ERROR) << "Exception at handler: " << e.what();
        return false;
    }
    return true;
}

bool SocketInterfaceTgBot::handle_CtrlSpamBlock(const void* ptr) {
    const auto* data = reinterpret_cast<const CtrlSpamBlock*>(ptr);
    gSpamBlockCfg = *data;
    return true;
}

bool SocketInterfaceTgBot::handle_ObserveChatId(const void* ptr) {
    const auto* data = reinterpret_cast<const ObserveChatId*>(ptr);
    auto obs = ChatObserver::getInstance();
    const std::lock_guard<std::mutex> _(obs->m);
    auto it = std::find(obs->observedChatIds.begin(),
                        obs->observedChatIds.end(), data->id);
    bool observe = data->observe;
    if (obs->observeAllChats) {
        LOG(WARNING) << "CMD_OBSERVE_CHAT_ID disabled due to "
                        "CMD_OBSERVE_ALL_CHATS";
        return false;
    }
    if (it == obs->observedChatIds.end()) {
        if (observe) {
            LOG(INFO) << "Adding chat to observer";
            obs->observedChatIds.push_back(data->id);
        } else {
            LOG(WARNING) << "Trying to quit observing chatid "
                         << "which wasn't being "
                            "observed!";
        }
    } else {
        if (observe) {
            LOG(WARNING) << "Trying to observe chatid "
                         << "which was already being "
                            "observed!";
        } else {
            LOG(INFO) << "Removing chat from observer";
            obs->observedChatIds.erase(it);
        }
    }
    return true;
}

bool SocketInterfaceTgBot::handle_SendFileToChatId(const void* ptr) {
    const auto* data = reinterpret_cast<const SendFileToChatId*>(ptr);
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
                return true;
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
        return false;
    }
    return true;
}

bool SocketInterfaceTgBot::handle_ObserveAllChats(const void* ptr) {
    auto obs = ChatObserver::getInstance();
    const std::lock_guard<std::mutex> _(obs->m);
    obs->observeAllChats = *reinterpret_cast<const ObserveAllChats*>(ptr);
    return true;
}

bool SocketInterfaceTgBot::handle_DeleteControllerById(const void* ptr) {
    DeleteControllerById data =
        *reinterpret_cast<const DeleteControllerById*>(ptr);
    enum SingleThreadCtrlManager::ThreadUsage threadUsage{};
    if (data < 0 || data >= SingleThreadCtrlManager::USAGE_MAX) {
        LOG(ERROR) << "Invalid controller id: " << data;
        return false;
    }
    threadUsage = static_cast<SingleThreadCtrlManager::ThreadUsage>(data);
    SingleThreadCtrlManager::getInstance()->destroyController(threadUsage);
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
    const void* ptr = pkt.data_ptr.getData();
    using namespace TgBotCommandData;
    bool ret = {};

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
        default:
            if (TgBotCmd::isClientCommand(pkt.header.cmd)) {
                LOG(ERROR) << "Unhandled cmd: "
                           << TgBotCmd::toStr(pkt.header.cmd);
            } else {
                LOG(WARNING) << "cmd ignored (as internal): "
                             << static_cast<int>(pkt.header.cmd);
            }
            ret = false;
            break;
    };
    switch (pkt.header.cmd) {
        case CMD_GET_UPTIME:
            // This has its own callback
            break;
        case CMD_WRITE_MSG_TO_CHAT_ID:
        case CMD_CTRL_SPAMBLOCK:
        case CMD_OBSERVE_CHAT_ID:
        case CMD_SEND_FILE_TO_CHAT_ID:
        case CMD_OBSERVE_ALL_CHATS:
        case CMD_DELETE_CONTROLLER_BY_ID:
            TgBotCommandPacket ackpkt(CMD_GENERIC_ACK, &ret, 1);
            LOG(INFO) << "Sending ack: " << std::boolalpha << ret;
            interface->writeToSocket(ctx, ackpkt.toSocketData());
            break;
    }
}
