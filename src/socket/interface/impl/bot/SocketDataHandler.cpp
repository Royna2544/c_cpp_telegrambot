#include <BotReplyMessage.h>
#include <ChatObserver.h>
#include <ResourceManager.h>
#include <SingleThreadCtrl.h>
#include <SocketConnectionHandler.h>
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

using TgBot::Api;
using TgBot::InputFile;
namespace fs = std::filesystem;

static std::string getMIMEString(const std::string& path) {
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

void socketConnectionHandler(const Bot& bot, struct TgBotCommandPacket pkt) {
    auto obs = ChatObserver::getInstance();
    static std::optional<std::chrono::system_clock::time_point> startTp;
    void* ptr = pkt.data_ptr.getData();
    using namespace TgBotCommandData;

    switch (pkt.header.cmd) {
        case CMD_WRITE_MSG_TO_CHAT_ID: {
            auto* data = reinterpret_cast<WriteMsgToChatId*>(ptr);
            try {
                bot_sendMessage(bot, data->to, data->msg);
            } catch (const TgBot::TgException& e) {
                LOG(ERROR) << "Exception at handler " << e.what();
            }
            break;
        }
        case CMD_CTRL_SPAMBLOCK:
            gSpamBlockCfg = *reinterpret_cast<CtrlSpamBlock*>(ptr);
            break;
        case CMD_OBSERVE_CHAT_ID: {
            const std::lock_guard<std::mutex> _(obs->m);
            auto* data = reinterpret_cast<ObserveChatId*>(ptr);
            auto it = std::find(obs->observedChatIds.begin(),
                                obs->observedChatIds.end(), data->id);
            bool observe = data->observe;
            if (obs->observeAllChats) {
                LOG(WARNING) << "CMD_OBSERVE_CHAT_ID disabled due to "
                                "CMD_OBSERVE_ALL_CHATS";
                break;
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
        } break;
        case CMD_SEND_FILE_TO_CHAT_ID: {
            auto *data = reinterpret_cast<SendFileToChatId*>(ptr);
            auto *file = data->filepath;
            using FileOrId_t = boost::variant<InputFile::Ptr, std::string>;
            std::function<Message::Ptr(const Api&, ChatId, FileOrId_t)> fn;
            try {
                switch (data->type) {
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
                            data->id, false, 0, nullptr,
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
                    fn(bot.getApi(), data->id,
                       InputFile::fromFile(file, getMIMEString(file)));
                } catch (std::ifstream::failure& e) {
                    LOG(INFO) << "Failed to send '" << file
                              << "' as local file, trying as Telegram "
                                 "file id";
                    fn(bot.getApi(), data->id, std::string(file));
                }
            } catch (const TgBot::TgException& e) {
                LOG(ERROR) << "Exception at handler, " << e.what();
            }
        } break;
        case CMD_OBSERVE_ALL_CHATS: {
            obs->observeAllChats = *reinterpret_cast<ObserveAllChats*>(ptr);
        } break;
        case CMD_DELETE_CONTROLLER_BY_ID: {
            int data = *reinterpret_cast<DeleteControllerById*>(ptr);
            enum SingleThreadCtrlManager::ThreadUsage threadUsage{};
            if (data < 0 || data >= SingleThreadCtrlManager::USAGE_MAX) {
                LOG(ERROR) << "Invalid controller id: " << data;
                break;
            }
            threadUsage =
                static_cast<SingleThreadCtrlManager::ThreadUsage>(data);
            SingleThreadCtrlManager::getInstance()->destroyController(
                threadUsage);
        } break;
        case CMD_SET_STARTTIME: {
            startTp = std::chrono::system_clock::from_time_t(
                *reinterpret_cast<SetStartTime*>(ptr));
            break;
        }
        case CMD_GET_UPTIME: {
            auto now = std::chrono::system_clock::now();
            if (startTp) {
                const auto diff = now - *startTp;
                const auto& dbwrapper = DefaultBotDatabase::getInstance();
                LOG(INFO) << "Uptime: " << to_string(diff);
                bot_sendMessage(bot, dbwrapper->getOwnerUserId(),
                                "Uptime is " + to_string(diff));
            } else {
                LOG(ERROR) << "StartTp is not set";
            }
            break;
        }
        default:
            if (TgBotCmd::isClientCommand(pkt.header.cmd)) {
                LOG(ERROR) << "Unhandled cmd: "
                           << TgBotCmd::toStr(pkt.header.cmd);
            } else {
                LOG(WARNING) << "cmd ignored (as internal): "
                             << static_cast<int>(pkt.header.cmd);
            }
            break;
    };
}
