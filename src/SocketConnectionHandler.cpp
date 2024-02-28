#include <BotReplyMessage.h>
#include <ChatObserver.h>
#include <Logging.h>
#include <ResourceIncBin.h>
#include <SocketConnectionHandler.h>
#include <SpamBlock.h>
#include <random/RandomNumberGenerator.h>
#include <rapidjson/document.h>
#include <rapidjson/rapidjson.h>

#include <filesystem>
#include <fstream>
#include <mutex>

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
        ASSIGN_INCTXT_DATA(MimeDataJson, buf);
        doc.Parse(buf.data());
        // This should be an assert, we know the data file at compile time
        ASSERT(!doc.HasParseError(), "Failed to parse mimedata: %d", doc.GetParseError());
    });
    if (!extension.empty()) {
        for (rapidjson::SizeType i = 0; i < doc.Size(); i++) {
            const rapidjson::Value& oneJsonElement = doc[i];
            const rapidjson::Value& availableTypes = oneJsonElement["types"].GetArray();
            for (rapidjson::SizeType i = 0; i < availableTypes.Size(); i++) {
                if (availableTypes[i].GetString() == extension) {
                    auto mime = oneJsonElement["name"].GetString();
                    LOG_D("Found MIME type: '%s'", mime);
                    return mime;
                }
            }
        }
        LOG_W("Unknown file extension: '%s'", extension.c_str());
    }
    return "application/octet-stream";
}

void socketConnectionHandler(const Bot& bot, struct TgBotConnection conn) {
    auto _data = conn.data;
    switch (conn.cmd) {
        case CMD_WRITE_MSG_TO_CHAT_ID:
            try {
                bot_sendMessage(bot, _data.data_1.to, _data.data_1.msg);
            } catch (const TgBot::TgException& e) {
                LOG_E("Exception at handler, %s", e.what());
            }
            break;
        case CMD_CTRL_SPAMBLOCK:
            gSpamBlockCfg = _data.data_3;
            break;
        case CMD_OBSERVE_CHAT_ID: {
            auto it = std::find(gObservedChatIds.begin(), gObservedChatIds.end(), _data.data_4.id);
            bool observe = _data.data_4.observe;
            if (gObserveAllChats) {
                LOG_W("CMD_OBSERVE_CHAT_ID disabled due to CMD_OBSERVE_ALL_CHATS");
                break;
            }
            if (it == gObservedChatIds.end()) {
                if (observe) {
                    LOG_D("Adding chat to observer");
                    gObservedChatIds.push_back(_data.data_4.id);
                } else {
                    LOG_W("Trying to quit observing chatid which wasn't being observed!");
                }
            } else {
                if (observe) {
                    LOG_W("Trying to observe chatid which was already being observed!");
                } else {
                    LOG_D("Removing chat from observer");
                    gObservedChatIds.erase(it);
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
                    case TYPE_DICE:
                        static const std::vector<std::string> dices = {"🎲", "🎯", "🏀", "⚽", "🎳", "🎰"};
                        // TODO: More clean code?
                        bot.getApi().sendDice(_data.data_5.id, false, 0, nullptr, dices[genRandomNumber(0, dices.size() - 1)]);
                        return;
                    default:
                        fn = [](const Api&, ChatId, const FileOrId_t) {
                            return nullptr;
                        };
                        break;
                }
                // Try to send as local file first
                try {
                    auto iFile = InputFile::fromFile(file, getMIMEString(file));
                    iFile->fileName = std::filesystem::path(file).filename().string();
                    fn(bot.getApi(), _data.data_5.id, iFile);
                } catch (std::ifstream::failure& e) {
                    LOG_I("Failed to send '%s' as local file, trying as Telegram file id", file);
                    fn(bot.getApi(), _data.data_5.id, std::string(file));
                }
            } catch (const TgBot::TgException& e) {
                LOG_E("Exception at handler, %s", e.what());
            }
        } break;
        case CMD_OBSERVE_ALL_CHATS: {
            gObserveAllChats = _data.data_6;
        } break;
        default:
            LOG_E("Unhandled cmd: %s", TgBotCmd_toStr(conn.cmd).c_str());
            break;
    };
}
