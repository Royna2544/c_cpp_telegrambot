#include <BotReplyMessage.h>
#include <ChatObserver.h>
#include <SocketConnectionHandler.h>
#include <SpamBlock.h>
#include <random/RandomNumberGenerator.h>
#include <rapidjson/document.h>
#include <rapidjson/rapidjson.h>

#include <filesystem>
#include <fstream>
#include <mutex>

#include "ResourceManager.h"
#include "SingleThreadCtrl.h"
#include "socket/TgBotSocket.h"
#include "tgbot/types/InputFile.h"

using TgBot::Api;
using TgBot::InputFile;
namespace fs = std::filesystem;

static std::string getMIMEString(const std::string& path) {
    static std::once_flag once;
    static rapidjson::Document doc;
    std::string extension = fs::path(path).extension().string();

    std::call_once(once, [] {
        std::string_view buf;
        buf = ResourceManager::getInstance().getResource("mimeData.json");
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
                    auto mime = oneJsonElement["name"].GetString();
                    LOG(INFO) << "Found MIME type: '" << mime << "'";
                    return mime;
                }
            }
        }
        LOG(WARNING) << "Unknown file extension: '" << extension << "'";
    }
    return "application/octet-stream";
}

void socketConnectionHandler(const Bot& bot, struct TgBotConnection conn) {
    auto _data = conn.data;
    auto& obs = ChatObserver::getInstance();

    switch (conn.cmd) {
        case CMD_WRITE_MSG_TO_CHAT_ID:
            try {
                bot_sendMessage(bot, _data.data_1.to, _data.data_1.msg);
            } catch (const TgBot::TgException& e) {
                LOG(ERROR) << "Exception at handler " << e.what();
            }
            break;
        case CMD_CTRL_SPAMBLOCK:
            gSpamBlockCfg = _data.data_3;
            break;
        case CMD_OBSERVE_CHAT_ID: {
            const std::lock_guard<std::mutex> _(obs.m);
            auto it = std::find(obs.observedChatIds.begin(),
                                obs.observedChatIds.end(), _data.data_4.id);
            bool observe = _data.data_4.observe;
            if (obs.observeAllChats) {
                LOG(WARNING) << "CMD_OBSERVE_CHAT_ID disabled due to "
                                "CMD_OBSERVE_ALL_CHATS";
                break;
            }
            if (it == obs.observedChatIds.end()) {
                if (observe) {
                    LOG(INFO) << "Adding chat to observer";
                    obs.observedChatIds.push_back(_data.data_4.id);
                } else {
                    LOG(WARNING)
                        << "Trying to quit observing chatid which wasn't being "
                           "observed!";
                }
            } else {
                if (observe) {
                    LOG(WARNING)
                        << "Trying to observe chatid which was already being "
                           "observed!";
                } else {
                    LOG(INFO) << "Removing chat from observer";
                    obs.observedChatIds.erase(it);
                }
            }
        } break;
        case CMD_SEND_FILE_TO_CHAT_ID: {
            auto file = _data.data_5.filepath;
            using FileOrId_t = boost::variant<InputFile::Ptr, std::string>;
            std::function<Message::Ptr(const Api&, ChatId, FileOrId_t)> fn;
            try {
                switch (_data.data_5.type) {
                    case TYPE_PHOTO:
                        fn = [](const Api& api, ChatId id,
                                const FileOrId_t file) {
                            return api.sendPhoto(id, file);
                        };
                        break;
                    case TYPE_VIDEO:
                        fn = [](const Api& api, ChatId id,
                                const FileOrId_t file) {
                            return api.sendVideo(id, file);
                        };
                        break;
                    case TYPE_GIF:
                        fn = [](const Api& api, ChatId id,
                                const FileOrId_t file) {
                            return api.sendAnimation(id, file);
                        };
                    case TYPE_DOCUMENT:
                        fn = [](const Api& api, ChatId id,
                                const FileOrId_t file) {
                            return api.sendDocument(id, file);
                        };
                        break;
                    case TYPE_DICE: {
                        static const std::vector<std::string> dices = {
                            "🎲", "🎯", "🏀", "⚽", "🎳", "🎰"};
                        // TODO: More clean code?
                        bot.getApi().sendDice(
                            _data.data_5.id, false, 0, nullptr,
                            dices[genRandomNumber(0, dices.size() - 1)]);
                        return;
                    }
                    default:
                        fn = [](const Api&, ChatId, const FileOrId_t) {
                            return nullptr;
                        };
                        break;
                }
                // Try to send as local file first
                try {
                    fn(bot.getApi(), _data.data_5.id,
                       InputFile::fromFile(file, getMIMEString(file)));
                } catch (std::ifstream::failure& e) {
                    LOG(INFO) << "Failed to send '" << file
                              << "' as local file, trying as Telegram "
                                 "file id";
                    fn(bot.getApi(), _data.data_5.id, std::string(file));
                }
            } catch (const TgBot::TgException& e) {
                LOG(ERROR) << "Exception at handler, " << e.what();
            }
        } break;
        case CMD_OBSERVE_ALL_CHATS: {
            obs.observeAllChats = _data.data_6;
        } break;
        case CMD_DELETE_CONTROLLER_BY_ID: {
            int data = _data.data_7;
            enum SingleThreadCtrlManager::ThreadUsage threadUsage {};
            if (data < 0 || data >= SingleThreadCtrlManager::USAGE_MAX) {
                LOG(ERROR) << "Invalid controller id: " << data;
                break;
            }
            threadUsage =
                static_cast<SingleThreadCtrlManager::ThreadUsage>(data);
            SingleThreadCtrlManager::getInstance().destroyController(
                threadUsage);
        } break;
        default:
            LOG(ERROR) << "Unhandled cmd: " << TgBotCmd::toStr(conn.cmd);
            break;
    };
}
