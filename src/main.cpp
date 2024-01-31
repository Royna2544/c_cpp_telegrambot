#include <Authorization.h>
#include <BotAddCommand.h>
#include <BotReplyMessage.h>
#include <CompilerInTelegram.h>
#include <ConfigManager.h>
#include <Database.h>
#include <ExtArgs.h>
#include <FileSystemLib.h>
#include <Logging.h>
#include <NamespaceImport.h>
#include <PrintableTime.h>
#include <RegEXHandler.h>
#include <ResourceIncBin.h>
#include <SpamBlock.h>
#include <TimerImpl.h>
#include <Types.h>
#include <popen_wdt/popen_wdt.h>
#include <random/RandomNumberGenerator.h>
#include <tgbot/tgbot.h>

#include <algorithm>
#include <boost/algorithm/string/replace.hpp>
#include <boost/config.hpp>
#include <chrono>
#include <exception>
#include <fstream>
#include <map>
#include <random>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "RuntimeException.h"
#include "StringToolsExt.h"
#include "socket/TgBotSocket.h"
#include "tgbot/TgException.h"
#include "tgbot/tools/StringTools.h"

#ifdef RTCOMMAND_LOADER
#include <RTCommandLoader.h>
#endif
#ifdef SOCKET_CONNECTION
#include <ChatObserver.h>
#include <SocketConnectionHandler.h>
#endif

#include "signalhandler/SignalHandler.h"

// tgbot
using TgBot::StickerSet;
using TgBot::TgLongPoll;

// Database.cpp
using database::blacklist;
using database::ProtoDatabase;
using database::whitelist;

int main(void) {
    std::string token;
    if (!ConfigManager::getVariable("TOKEN", token)) {
        LOG_F("Failed to get TOKEN variable");
        return EXIT_FAILURE;
    }
    static Bot gBot(token);

    database::db.load();
    static auto ctx = std::make_shared<TimerCtx>();

    bot_AddCommandEnforcedCompiler(gBot, "c", ProgrammingLangs::C, [](const Bot &bot, const Message::Ptr &message, std::string compiler) {
        CompileRunHandler(CCppCompileHandleData{{{bot, message}, compiler, "out.c"}});
    });
    bot_AddCommandEnforcedCompiler(gBot, "cpp", ProgrammingLangs::CXX, [](const Bot &bot, const Message::Ptr &message, std::string compiler) {
        CompileRunHandler(CCppCompileHandleData{{{bot, message}, compiler, "out.cpp"}});
    });
    bot_AddCommandEnforcedCompiler(gBot, "python", ProgrammingLangs::PYTHON, [](const Bot &bot, const Message::Ptr &message, std::string compiler) {
        CompileRunHandler({{bot, message}, compiler, "out.py"});
    });
    bot_AddCommandEnforcedCompiler(gBot, "golang", ProgrammingLangs::GO, [](const Bot &bot, const Message::Ptr &message, std::string compiler) {
        CompileRunHandler({{bot, message}, compiler + " run", "out.go"});
    });

    bot_AddCommandEnforced(gBot, "addblacklist",
                           std::bind(&ProtoDatabase::addToDatabase, blacklist, pholder1, pholder2));
    bot_AddCommandEnforced(gBot, "rmblacklist",
                           std::bind(&ProtoDatabase::removeFromDatabase, blacklist, pholder1, pholder2));
    bot_AddCommandEnforced(gBot, "addwhitelist",
                           std::bind(&ProtoDatabase::addToDatabase, whitelist, pholder1, pholder2));
    bot_AddCommandEnforced(gBot, "rmwhitelist",
                           std::bind(&ProtoDatabase::removeFromDatabase, whitelist, pholder1, pholder2));

    bot_AddCommandEnforced(gBot, "bash", [](const Bot &bot, const Message::Ptr message) {
        CompileRunHandler(BashHandleData{{bot, message}, false});
    });
    bot_AddCommandEnforced(gBot, "unsafebash", [](const Bot &bot, const Message::Ptr message) {
        CompileRunHandler(BashHandleData{{bot, message}, true});
    });
    bot_AddCommandPermissive(gBot, "alive", [](const Bot &bot, const Message::Ptr &message) {
        static std::string version;
        static std::once_flag once;

        std::call_once(once, [] {
            std::string commitid, commitmsg, originurl, compilerver;

            static const std::map<std::string *, std::string> commands = {
                {&commitid, "git rev-parse HEAD"},
                {&commitmsg, "git log --pretty=%s -1"},
                {&originurl, "git config --get remote.origin.url"},
            };
            for (const auto &cmd : commands) {
                const bool ret = runCommand(cmd.second, *cmd.first);
                if (!ret) {
                    throw runtime_errorf("Command failed: %s", cmd.second.c_str());
                }
            }
            compilerver = std::string(BOOST_PLATFORM " | " BOOST_COMPILER " | " __DATE__);
            ASSIGN_INCTXT_DATA(AboutHtmlText, version);
#define REPLACE_PLACEHOLDER(buf, name) boost::replace_all(buf, "_" #name "_", name)
            REPLACE_PLACEHOLDER(version, commitid);
            REPLACE_PLACEHOLDER(version, commitmsg);
            REPLACE_PLACEHOLDER(version, originurl);
            REPLACE_PLACEHOLDER(version, compilerver);
        });
        try {
            // Hardcoded kys GIF
            bot.getApi().sendAnimation(message->chat->id,
                                    "CgACAgIAAx0CdMESqgACCZRlrfMoq_b2DL21k6ohShQzzLEh6gACsw4AAuSZWUmmR3jSJA9WxzQE",
                                    0, 0, 0, "", version, message->messageId,
                                    nullptr, "html");
        } catch (const TgBot::TgException& e) {
            // Fallback to HTML if no GIF
            LOG_E("Alive cmd: Error while sending GIF: %s", e.what());
            bot_sendReplyMessageHTML(bot, message, version);
        }

    });
    bot_AddCommandPermissive(gBot, "flash", [](const Bot &bot, const Message::Ptr &message) {
        static std::vector<std::string> reasons;
        static std::once_flag once;
        static std::regex kFlashTextRegEX(R"(Flashing '\S+.zip'\.\.\.)");
        static const char kZipExtentionSuffix[] = ".zip";
        std::string msg;
        std::stringstream ss;
        Message::Ptr sentmsg;

        std::call_once(once, [] {
            std::string buf, line;
            std::stringstream ss;

            ASSIGN_INCTXT_DATA(FlashTxt, buf);
            splitAndClean(buf, reasons);
        });

        if (message->replyToMessage != nullptr) {
            msg = message->replyToMessage->text;
            if (msg.empty()) {
                bot_sendReplyMessage(bot, message, "Reply to a text");
                return;
            }
        } else {
            if (!hasExtArgs(message)) {
                bot_sendReplyMessage(bot, message, "Send a file name");
                return;
            }
            parseExtArgs(message, msg);
        }
        if (msg.find('\n') != std::string::npos) {
            bot_sendReplyMessage(bot, message, "Invalid input: Zip names shouldn't have newlines");
            return;
        }
        std::replace(msg.begin(), msg.end(), ' ', '_');
        if (!StringTools::endsWith(msg, kZipExtentionSuffix)) {
            msg += kZipExtentionSuffix;
        }
        ss << "Flashing '" << msg << "'..." << std::endl;
        sentmsg = bot_sendReplyMessage(bot, message, ss.str());
        std_sleep_s(genRandomNumber(5));
        if (const size_t pos = genRandomNumber(reasons.size()); pos != reasons.size()) {
            ss << "Failed successfully!" << std::endl;
            ss << "Reason: " << reasons[pos];
        } else {
            ss << "Success! Chance was 1/" << reasons.size();
        }
        bot_editMessage(bot, sentmsg, ss.str());
    });
    bot_AddCommandPermissive(gBot, "possibility", [](const Bot &bot, const Message::Ptr &message) {
        constexpr int PERCENT_MAX = 100;
        if (!hasExtArgs(message)) {
            bot_sendReplyMessage(bot, message,
                                 "Send avaliable conditions sperated by newline");
            return;
        }
        std::string text;
        parseExtArgs(message, text);
        std::stringstream ss(text), out;
        std::string last;
        std::unordered_map<std::string, int> map;
        std::vector<std::string> vec;

        splitAndClean(text, vec);
        map.reserve(vec.size());
        if (vec.size() == 1) {
            bot_sendReplyMessage(bot, message, "Give more than 1 choice");
            return;
        }
        shuffleStringArray(vec);
        out << "Total " << vec.size() << " items" << std::endl;
        last = vec.back();
        vec.pop_back();
        int total = 0;
        for (const auto &cond : vec) {
            int thisper = 0;
            if (total < PERCENT_MAX) {
                thisper = genRandomNumber(PERCENT_MAX - total);
                if (total + thisper >= PERCENT_MAX) {
                    thisper = PERCENT_MAX - total;
                }
            }
            map[cond] = thisper;
            total += thisper;
        }
        // Nonetheless of total being 100 or whatever
        map[last] = PERCENT_MAX - total;
        using map_t = std::pair<std::string, int>;
        std::vector<map_t> elem(map.begin(), map.end());
        std::sort(elem.begin(), elem.end(), [](const map_t &map1, const map_t &map2) {
            if (map1.second != map2.second) {
                return map1.second > map2.second;
            }
            return map1.first < map2.first;
        });
        for (const map_t &m : elem) {
            out << m.first << " : " << m.second << "%" << std::endl;
        }
        bot_sendReplyMessage(bot, message, out.str());
    });
    bot_AddCommandPermissive(gBot, "delay", [](const Bot &bot, const Message::Ptr &message) {
        using std::chrono::high_resolution_clock;
        using std::chrono::duration;
        union time now {
            .val = time(nullptr)
        }, msg{.val = message->date};
        std::ostringstream ss;

        ss << "Request message sent at: " << msg << std::endl;
        ss << "Received at: " << now << " Diff: " << now - msg << 's' << std::endl;
        auto beforeSend = high_resolution_clock::now();
        auto sentMsg = bot_sendReplyMessage(bot, message, ss.str());
        auto afterSend = high_resolution_clock::now();
        ss << "Sending reply message took: " << duration<double, std::milli>(afterSend - beforeSend).count() << "ms" << std::endl;
        bot_editMessage(bot, sentMsg, ss.str());
    });
    bot_AddCommandEnforced(gBot, "starttimer", std::bind(startTimer, pholder1, pholder2, ctx));
    bot_AddCommandEnforced(gBot, "stoptimer", std::bind(stopTimer, pholder1, pholder2, ctx));
    bot_AddCommandPermissive(gBot, "decho", [](const Bot &bot, const Message::Ptr &message) {
        const auto replyMsg = message->replyToMessage;
        const auto chatId = message->chat->id;
        static std::vector<ChatId> warned_ids;

        try {
            bot.getApi().deleteMessage(chatId, message->messageId);
        } catch (const TgBot::TgException &) {
            // bot is not admin. nothing it can do
            if (std::find(warned_ids.begin(), warned_ids.end(), chatId) == warned_ids.end()) {
                LOG_W("bot is not admin in chat " LONGFMT ", cannot use decho!",
                      message->chat->id);
                warned_ids.emplace_back(message->chat->id);
            }
            return;
        }
        if (hasExtArgs(message)) {
            std::string msg;
            parseExtArgs(message, msg);
            bot_sendReplyMessage(bot, message, msg,
                                 (replyMsg) ? replyMsg->messageId : 0, true);
        } else if (replyMsg) {
            if (replyMsg->sticker) {
                bot.getApi().sendSticker(message->chat->id, replyMsg->sticker->fileId);
            } else if (replyMsg->animation) {
                bot.getApi().sendAnimation(message->chat->id, replyMsg->animation->fileId);
            } else if (replyMsg->video) {
                bot.getApi().sendVideo(message->chat->id, replyMsg->video->fileId);
            } else if (!replyMsg->photo.empty()) {
                bot.getApi().sendPhoto(message->chat->id, replyMsg->photo.front()->fileId,
                                       "(Note: Sending all photos are not supported)");
            } else if (!replyMsg->text.empty()) {
                bot_sendReplyMessage(bot, message, replyMsg->text, replyMsg->messageId);
            }
        }
    });
    bot_AddCommandPermissive(gBot, "randsticker", [](const Bot &bot, const Message::Ptr &message) {
        if (message->replyToMessage && message->replyToMessage->sticker) {
            StickerSet::Ptr stickset;
            try {
                stickset = bot.getApi().getStickerSet(
                    message->replyToMessage->sticker->setName);
            } catch (const std::exception &e) {
                bot_sendReplyMessage(bot, message, e.what());
                return;
            }
            const ssize_t pos = genRandomNumber(stickset->stickers.size() - 1);
            bot.getApi().sendSticker(message->chat->id,
                                     stickset->stickers[pos]->fileId,
                                     message->messageId, nullptr, false, true);
            std::stringstream ss;
            ss << "Sticker idx: " << pos + 1
               << " emoji: " << stickset->stickers[pos]->emoji << std::endl
               << "From pack \"" + stickset->title + "\"";
            bot_sendReplyMessage(bot, message, ss.str());
        } else {
            bot_sendReplyMessage(bot, message, "Sticker not found in replied-to message");
        }
    });
    bot_AddCommandPermissive(gBot, "fileid", [](const Bot &bot, const Message::Ptr &message) {
        const auto replyMsg = message->replyToMessage;
        std::string file, unifile;

        if (replyMsg) {
            if (replyMsg->sticker) {
                file = replyMsg->sticker->fileId;
                unifile = replyMsg->sticker->fileUniqueId;
            } else if (replyMsg->animation) {
                file = replyMsg->animation->fileId;
                unifile = replyMsg->animation->fileUniqueId;
            } else if (replyMsg->video) {
                file = replyMsg->video->fileId;
                unifile = replyMsg->video->fileUniqueId;
            } else if (replyMsg->photo.size() == 1) {
                file = replyMsg->photo.front()->fileId;
                unifile = replyMsg->photo.front()->fileUniqueId; 
            } else {
                file = unifile = "Unknown";
            }
            bot_sendReplyMessageMarkDown(bot, message, "FileId: `" + file + "`\n" + "FileUniqueId: `" + unifile + '`');
        } else {
            bot_sendReplyMessage(bot, message, "Reply to a media");
        }
    });
    gBot.getEvents().onAnyMessage([](const Message::Ptr &msg) {
        if (!gAuthorized) return;
#ifdef SOCKET_CONNECTION
        if (!gObservedChatIds.empty() || gObserveAllChats)
            processObservers(msg);
#endif
        spamBlocker(gBot, msg);
    });

#ifdef SOCKET_CONNECTION
    static std::thread th;
    static std::atomic_bool socketValid = true;
    static std::string exitToken;

    th = std::thread([] {
        socketValid = startListening([](struct TgBotConnection conn) {
            socketConnectionHandler(gBot, conn);
        });
    });
    exitToken = StringTools::generateRandomString(sizeof(TgBotCommandUnion::data_2.token) - 1);
    LOG_D("Generated token: %s", exitToken.c_str());
    
    TgBotCommandData::Exit e;
    e.op = ExitOp::SET_TOKEN;
    strncpy(e.token, exitToken.c_str(), sizeof(e.token) - 1);
    e.token[sizeof(e.token) - 1] = 0;

    std_sleep_s(3);
    writeToSocket({CMD_EXIT, {.data_2 = e}});
#endif
#ifdef RTCOMMAND_LOADER
    loadCommandsFromFile(gBot, getSrcRoot() / "modules.load");
#endif
    static auto cleanupFn = [](int s) {
        static std::once_flag once;
        std::call_once(once, [s] {
            LOG_I("Exiting with signal %d", s);
            forceStopTimer(ctx);
            database::db.save();
#ifdef SOCKET_CONNECTION
            std::error_code ec;
            if (!std::filesystem::exists(SOCKET_PATH, ec)) {
                LOG_W("Socket file was deleted");
                socketValid = false;
            }
            if (socketValid) {
                if (th.joinable()) {
                    TgBotCommandData::Exit e;
                    e.op = ExitOp::DO_EXIT;
                    strncpy(e.token, exitToken.c_str(), sizeof(e.token) - 1);
                    e.token[sizeof(e.token) - 1] = 0;
                    writeToSocket({CMD_EXIT, {.data_2 = e}});
                    th.join();
                }
            } else {
                if (th.joinable()) {
                    forceStopListening();
                    th.join();
                }
            }
#endif
        });
        std::exit(0);
    };
    installSignalHandler(cleanupFn);
    int64_t lastcrash = 0;

    LOG_D("Token: %s", token.c_str());
reinit:
    try {
        LOG_D("Bot username: %s", gBot.getApi().getMe()->username.c_str());
        gBot.getApi().deleteWebhook();

        TgLongPoll longPoll(gBot);
        while (true) {
            longPoll.start();
        }
    } catch (const std::exception &e) {
        LOG_E("Exception: %s", e.what());
        LOG_W("Trying to recover");
        UserId ownerid = database::db.maybeGetOwnerId();
        try {
            gBot.getApi().sendMessage(ownerid, std::string() + "Exception occured: " + e.what());
        } catch (const std::exception &e) {
            LOG_F("%s", e.what());
            goto exit;
        }
        const int64_t temptime = time(nullptr);
        if (temptime - lastcrash < 15 && lastcrash != 0) {
            gBot.getApi().sendMessage(ownerid, "Recover failed.");
            LOG_F("Recover failed");
            goto exit;
        }
        lastcrash = temptime;
        gBot.getApi().sendMessage(ownerid, "Reinitializing.");
        LOG_I("Re-init");
        gAuthorized = false;
        std::thread([] {
            std_sleep(5s);
            gAuthorized = true;
        }).detach();
        goto reinit;
    }
exit:
    cleanupFn(-1);
    return EXIT_FAILURE;
}
