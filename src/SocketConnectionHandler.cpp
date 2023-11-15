#include <ChatObserver.h>
#include <Logging.h>
#include <SocketConnectionHandler.h>
#include <SpamBlock.h>
#include <utils/libutils.h>

#include <fstream>
#include "tgbot/types/InputFile.h"

using TgBot::Api;
using TgBot::InputFile;

void socketConnectionHandler(const Bot& bot, struct TgBotConnection conn) {
    auto _data = conn.data;
    switch (conn.cmd) {
        case CMD_WRITE_MSG_TO_CHAT_ID:
            try {
                bot.getApi().sendMessage(_data.data_1.to, _data.data_1.msg);
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
                    default:
                        fn = [](const Api&, ChatId, const FileOrId_t) {
                            return nullptr;
                        };
                        break;
                }
                // Try to send as local file first
                try {
                    fn(bot.getApi(), _data.data_5.id, InputFile::fromFile(file, getMIMEString(file)));
                } catch (std::ifstream::failure& e) {
                    LOG_I("Failed to send '%s' as local file, trying as Telegram file id", file);
                    fn(bot.getApi(), _data.data_5.id, std::string(file));
                }
            } catch (const TgBot::TgException& e) {
                LOG_E("Exception at handler, %s", e.what());
            }
        } break;
        default:
            LOG_E("Unhandled cmd: %s", toStr(conn.cmd).c_str());
            break;
    };
}
