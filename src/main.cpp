#include <Authorization.h>
#include <BotAddCommand.h>
#include <BotReplyMessage.h>
#include <CompilerInTelegram.h>
#include <Database.h>
#include <ExtArgs.h>
#include <Logging.h>
#include <NamespaceImport.h>
#include <PrintableTime.h>
#include <RegEXHandler.h>
#include <SpamBlock.h>
#include <TimerImpl.h>
#include <Types.h>
#include <tgbot/tgbot.h>
#include <utils/LinuxPort.h>

#include <algorithm>
#include <boost/algorithm/string/replace.hpp>
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
#include "tgbot/TgException.h"
#include "tgbot/tools/StringTools.h"

#ifdef RTCOMMAND_LOADER
#include <RTCommandLoader.h>
#endif
#ifdef SOCKET_CONNECTION
#include <ChatObserver.h>
#include <SocketConnectionHandler.h>
#endif

#include "exithandlers/handler.h"
#include "utils/libutils.h"

// tgbot
using TgBot::StickerSet;
using TgBot::TgLongPoll;

// Database.cpp
using database::blacklist;
using database::ProtoDatabase;
using database::whitelist;

using std::chrono_literals::operator""s;

int main(void) {
    const char *token_str = getenv("TOKEN");
    std::string token;
    if (!token_str) {
        std::string home, line;
        LOG_W("TOKEN is not exported, try config file");
        if (!getHomePath(home)) {
            LOG_E("Cannot find HOME");
            return EXIT_FAILURE;
        }
        const std::string confPath = home + dir_delimiter + ".tgbot_token";
        std::ifstream ifs(confPath);
        if (ifs.fail()) {
            LOG_E("Opening %s failed", confPath.c_str());
            return EXIT_FAILURE;
        }
        std::getline(ifs, line);
        if (line.empty()) {
            LOG_E("Conf file %s empty", confPath.c_str());
            return EXIT_FAILURE;
        }
        token = line;
    } else
        token = token_str;

    static Bot gBot(token);

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
            compilerver = getCompileVersion();
            ReadFileToString(getResourcePath("about.html.txt"), &version);
#define REPLACE_PLACEHOLDER(buf, name) boost::replace_all(buf, "_" #name "_", name)
            REPLACE_PLACEHOLDER(version, commitid);
            REPLACE_PLACEHOLDER(version, commitmsg);
            REPLACE_PLACEHOLDER(version, originurl);
            REPLACE_PLACEHOLDER(version, compilerver);
        });
        // Hardcoded Cum about it GIF
        bot.getApi().sendAnimation(message->chat->id,
                                   "CgACAgQAAx0CdY7-CgABARtpZLefgyKNpSLvyCJWcp8"
                                   "mt5KF_REAAgkDAAI2tFRQIk0uTVxfZnsvBA",
                                   0, 0, 0, "", version, message->messageId,
                                   nullptr, "html");
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

            ReadFileToString(getResourcePath("flash.txt"), &buf);
            ss = std::stringstream(buf);
            while (std::getline(ss, line)) {
                if (!line.empty())
                    reasons.emplace_back(line);
            }
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
        for (const auto c : msg) {
            if (c == '\n') {
                bot_sendReplyMessage(bot, message, "Zip names shouldn't have newlines");
                return;
            }
        }
        if (!std::regex_match(StringTools::split(msg, '\n').front(), kFlashTextRegEX)) {
            std::replace(msg.begin(), msg.end(), ' ', '_');
            if (!StringTools::endsWith(msg, kZipExtentionSuffix)) {
                msg += kZipExtentionSuffix;
            }
            ss << "Flashing '" << msg << "'..." << std::endl;
            sentmsg = bot_sendReplyMessage(bot, message, ss.str());
            std_sleep(std::chrono::seconds(genRandomNumber(5)));
            if (const size_t pos = genRandomNumber(reasons.size()); pos != reasons.size()) {
                ss << "Failed successfully!" << std::endl;
                ss << "Reason: " << reasons[pos];
            } else {
                ss << "Success! Chance was 1/" << reasons.size();
            }
            bot_editMessage(bot, sentmsg, ss.str());
        } else {
            bot_sendReplyMessage(bot, message, "Do not try to recurse flash command, Else I'll nuke u");
        }
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
        std::string line, last;
        std::vector<std::string> vec;
        std::unordered_map<std::string, int> map;
        std::random_device rd;
        std::mt19937 gen(rd());

        int numlines = 1;
        for (const char c : message->text) {
            if (c == '\n') numlines++;
        }
        vec.reserve(numlines);
        map.reserve(numlines);
        while (std::getline(ss, line, '\n')) {
            if (std::all_of(line.begin(), line.end(),
                            [](char c) { return std::isspace(c); }))
                continue;
            vec.push_back(line);
        }
        if (vec.size() == 1) {
            bot_sendReplyMessage(bot, message, "Give more than 1 choice");
            return;
        }
        std::shuffle(vec.begin(), vec.end(), gen);
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
    bot_AddCommandEnforced(gBot, "starttimer", startTimer);
    bot_AddCommandEnforced(gBot, "stoptimer", stopTimer);
    bot_AddCommandPermissive(gBot, "decho", [](const Bot &bot, const Message::Ptr &message) {
        const auto replyMsg = message->replyToMessage;
        try {
            bot.getApi().deleteMessage(message->chat->id, message->messageId);
        } catch (const TgBot::TgException &) {
            // bot is not admin. nothing it can do
            WARN_ONCE(1, "bot is not admin in chat " LONGFMT ", cannot use decho!",
                      message->chat->id);
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
    gBot.getEvents().onAnyMessage([](const Message::Ptr &msg) {
#ifdef SOCKET_CONNECTION
        if (!gObservedChatIds.empty())
            processObservers(msg);
#endif
        if (!msg->text.empty())
            processRegEXCommand(gBot, msg);
        spamBlocker(gBot, msg);
    });

#ifdef SOCKET_CONNECTION
    static std::thread th;
    th = std::thread([] {
        startListening([](struct TgBotConnection conn) {
            socketConnectionHandler(gBot, conn);
        });
    });
#endif
#ifdef RTCOMMAND_LOADER
    loadCommandsFromFile(gBot, getSrcRoot() + "/modules.load");
#endif
    static bool exited = false;
    static auto cleanupFunc = [](int s) {
        if (exited) return;
        LOG_I("Exiting with signal %d", s);
        forceStopTimer();
        database::db.save();
#ifdef SOCKET_CONNECTION
        if (th.joinable()) {
            writeToSocket({CMD_EXIT, {.data_2 = nullptr}});
            th.join();
        }
#endif
        exited = true;
        std::exit(0);
    };
    installExitHandler(cleanupFunc);
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
            gBot.getApi().sendMessage(ownerid, e.what());
        } catch (const std::exception &e) {
            LOG_F("%s", e.what());
            return EXIT_FAILURE;
        }
        const int64_t temptime = time(nullptr);
        if (temptime - lastcrash < 15 && lastcrash != 0) {
            gBot.getApi().sendMessage(ownerid, "Recover failed.");
            LOG_F("Recover failed");
            return EXIT_FAILURE;
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
}
