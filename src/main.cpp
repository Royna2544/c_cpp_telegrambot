#include <fcntl.h>
#include <tgbot/tgbot.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <vector>

#ifdef USE_DATABASE
#include <Database.h>
#endif

#ifdef __WIN32
#include <windows/sighandler.h>
#endif

#include <Authorization.h>
#include <BotAddCommand.h>
#include <BotReplyMessage.h>
#include <CompilerInTelegram.h>
#include <NamespaceImport.h>
#include <Timer.h>

#include "utils/libutils.h"

using std::chrono_literals::operator""s;

struct TimerImpl_privdata {
    int32_t messageid;
    const TgBot::Bot &bot;
    bool botcanpin, sendendmsg;
    int64_t chatid;
};

// tgbot
using TgBot::MessageEntity;
using TgBot::StickerSet;
using TgBot::TgLongPoll;
// stdc++
static inline auto pholder1 = std::placeholders::_1;
static inline auto pholder2 = std::placeholders::_2;
// Database.cpp
using database::blacklist;
using database::ProtoDatabase;
using database::whitelist;

int main(void) {
    const char *token_str = getenv("TOKEN");
    std::string token;
    if (!token_str) {
        PRETTYF("Warning: TOKEN is not exported, try config file");
        std::string home;
        if (!getHomePath(home)) {
            PRETTYF("Error: Cannot find HOME");
            return EXIT_FAILURE;
        }
        std::string confPath = home + dir_delimiter + ".tgbot_token", line;
        std::ifstream ifs(confPath);
        if (ifs.fail()) {
            PRETTYF("Error: Opening %s failed", confPath.c_str());
            return EXIT_FAILURE;
        }
        std::getline(ifs, line);
        if (line.empty()) {
            PRETTYF("Error: Conf file %s empty", confPath.c_str());
            return EXIT_FAILURE;
        }
        token = line;
    } else
        token = token_str;

    database::db->set_ownerid(ownerid);
    Bot gbot(token);
    static std::shared_ptr<Timer<TimerImpl_privdata>> tm_ptr;
    std::string CCompiler, CXXCompiler, GoCompiler, PythonInterpreter;
    CCompiler = findCompiler(ProgrammingLangs::C);
    CXXCompiler = findCompiler(ProgrammingLangs::CXX);
    GoCompiler = findCompiler(ProgrammingLangs::GO);
    PythonInterpreter = findCompiler(ProgrammingLangs::PYTHON);

    if (!CXXCompiler.empty()) {
        bot_AddCommandEnforced(gbot, "cpp", [&](const Bot &bot, const Message::Ptr &message) {
            CompileRunHandler<CCppCompileHandleData>({bot, message, CXXCompiler, "compile.cpp"});
        });
    } else {
        NOT_SUPPORTED_COMPILER(gbot, "cpp");
    }
    if (!CCompiler.empty()) {
        bot_AddCommandEnforced(gbot, "c", [&](const Bot &bot, const Message::Ptr &message) {
            CompileRunHandler<CCppCompileHandleData>({bot, message, CCompiler, "compile.c"});
        });
    } else {
        NOT_SUPPORTED_COMPILER(gbot, "c");
    }
    if (!PythonInterpreter.empty()) {
        bot_AddCommandEnforced(gbot, "python", [&](const Bot &bot, const Message::Ptr &message) {
            CompileRunHandler({bot, message, PythonInterpreter, "./out.py"});
        });
    } else {
        NOT_SUPPORTED_COMPILER(gbot, "python");
    }
    if (!GoCompiler.empty()) {
        bot_AddCommandEnforced(gbot, "golang", [&](const Bot &bot, const Message::Ptr &message) {
            CompileRunHandler({bot, message, GoCompiler + " run", "./out.go"});
        });
    } else {
        NOT_SUPPORTED_COMPILER(gbot, "golang");
    }
#ifdef USE_DATABASE
    bot_AddCommandEnforced(gbot, "addblacklist",
                           std::bind(&ProtoDatabase::addToDatabase, blacklist, pholder1, pholder2));
    bot_AddCommandEnforced(gbot, "rmblacklist",
                           std::bind(&ProtoDatabase::removeFromDatabase, blacklist, pholder1, pholder2));
    bot_AddCommandEnforced(gbot, "addwhitelist",
                           std::bind(&ProtoDatabase::addToDatabase, whitelist, pholder1, pholder2));
    bot_AddCommandEnforced(gbot, "rmwhitelist",
                           std::bind(&ProtoDatabase::removeFromDatabase, whitelist, pholder1, pholder2));
    bot_AddCommandEnforced(gbot, "savedb", [](const Bot &bot, const Message::Ptr &message) {
        database::db.save();
        bot_sendReplyMessage(bot, message, "OK");
    });
#else
    NOT_SUPPORTED_DB("addblacklist");
    NOT_SUPPORTED_DB("rmblacklist");
    NOT_SUPPORTED_DB("addwhitelist");
    NOT_SUPPORTED_DB("rmwhitelist");
#endif
    bot_AddCommandEnforced(gbot, "bash", [](const Bot &bot, const Message::Ptr &message) {
        CompileRunHandler(BashHandleData{bot, message, false});
    });
    bot_AddCommandEnforced(gbot, "unsafebash", [](const Bot &bot, const Message::Ptr &message) {
        CompileRunHandler(BashHandleData{bot, message, true});
    });
    bot_AddCommandPermissive(gbot, "alive", [](const Bot &bot, const Message::Ptr &message) {
        static std::string version_str;
        static std::once_flag once;
        std::call_once(once, [] {
            char buf[1024] = {0};
            std::string commitid, commitmsg, originurl;
            std::string buffer;

            static const std::map<std::string *, std::string> commands = {
                {&commitid, "git rev-parse HEAD"},
                {&commitmsg, "git log --pretty=%s -1"},
                {&originurl, "git config --get remote.origin.url"},
            };
            for (const auto &cmd : commands) {
                bool ret = runCommand(cmd.second, *cmd.first);
                if (!ret) {
                    *cmd.first = "(Command failed)";
                }
            }
            ReadFileToString(getResourcePath("about.html.txt"), &buffer);
            snprintf(buf, sizeof(buf), buffer.c_str(), commitmsg.c_str(), originurl.c_str(),
                     commitid.c_str(), commitid.c_str(), originurl.c_str(), getCompileVersion().c_str());
            version_str = buf;
        });
        // Hardcoded Cum about it GIF
        bot.getApi().sendAnimation(message->chat->id,
                                   "CgACAgQAAx0CdY7-CgABARtpZLefgyKNpSLvyCJWcp8"
                                   "mt5KF_REAAgkDAAI2tFRQIk0uTVxfZnsvBA",
                                   0, 0, 0, "", version_str, message->messageId,
                                   nullptr, "html");
    });
    bot_AddCommandPermissive(gbot, "flash", [](const Bot &bot, const Message::Ptr &message) {
        static std::vector<std::string> reasons;
        static std::once_flag once;
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
        std::string msg = message->text;
        if (message->replyToMessage != nullptr) {
            msg = message->replyToMessage->text;

        } else {
            if (!hasExtArgs(message)) {
                bot_sendReplyMessage(bot, message, "Send a file name");
                return;
            }
            parseExtArgs(message, msg);
        }
        if (msg.empty()) {
            bot_sendReplyMessage(bot, message, "Reply to a text");
            return;
        }
        std::replace(msg.begin(), msg.end(), ' ', '_');
        std::stringstream ss;
        ss << "Flashing '" << msg;
        if (msg.find(".zip") == std::string::npos) ss << ".zip";
        ssize_t pos = genRandomNumber(reasons.size());
        if (pos != reasons.size()) {
            ss << "' failed successfully!" << std::endl;
            ss << "Reason: " << reasons[pos];
        } else {
            ss << "' Success! Chance was 1/" << reasons.size();
        }
        bot_sendReplyMessage(bot, message, ss.str());
    });
    bot_AddCommandPermissive(gbot, "possibility", [](const Bot &bot, const Message::Ptr &message) {
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
        for (char c : message->text) {
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
            int thisper = genRandomNumber(100 - total);
            map[cond] = thisper;
            total += thisper;
        }
        map[last] = 100 - total;
        using map_t = std::pair<std::string, int>;
        std::vector<map_t> elem(map.begin(), map.end());
        std::sort(elem.begin(), elem.end(), [](const map_t &map1, const map_t &map2) {
            return map1.second <= map2.second;
        });
        for (const map_t &m : elem) {
            out << m.first << " : " << m.second << "%" << std::endl;
        }
        bot_sendReplyMessage(bot, message, out.str());
    });
    bot_AddCommandEnforced(gbot, "starttimer", [](const Bot &bot, const Message::Ptr &message) {
        enum InputState {
            HOUR,
            MINUTE,
            SECOND,
            NONE,
        };
        bool found = false;
        std::string msg;
        if (hasExtArgs(message)) {
            parseExtArgs(message, msg);
            found = true;
        }
        if (message->replyToMessage != nullptr) {
            msg = message->replyToMessage->text;
            found = true;
        }
        if (!found) {
            bot_sendReplyMessage(bot, message, "Send or reply to a time, in hhmmss format");
            return;
        }
        if (tm_ptr && tm_ptr->isrunning()) {
            bot_sendReplyMessage(bot, message,
                                 "Timer is already running");
            return;
        }
        const char *c_str = msg.c_str();
        std::vector<int> numbercache;
        enum InputState state;
        struct timehms hms = {0};
        for (int i = 0; i <= msg.size(); i++) {
            int code = static_cast<int>(c_str[i]);
            if (i == msg.size()) code = ' ';
            switch (code) {
                case ' ': {
                    if (numbercache.size() != 0) {
                        int result = 0, count = 1;
                        for (const auto i : numbercache) {
                            result += pow(10, numbercache.size() - count) * i;
                            count++;
                        }
                        switch (state) {
                            case InputState::HOUR:
                                hms.h = result;
                                break;
                            case InputState::MINUTE:
                                hms.m = result;
                                break;
                            case InputState::SECOND:
                                hms.s = result;
                                break;
                            default:
                                break;
                        }
#ifndef NDEBUG
                        PRETTYF("result: %d", result);
#endif
                        state = InputState::NONE;
                        numbercache.clear();
                    }
                    break;
                }
                case '0' ... '9': {
                    int intver = code - 48;
#ifndef NDEBUG
                    PRETTYF("%d", intver);
#endif
                    numbercache.push_back(intver);
                    break;
                }
                case 'H':
                case 'h': {
                    state = InputState::HOUR;
                    break;
                }
                case 'M':
                case 'm': {
                    state = InputState::MINUTE;
                    break;
                }
                case 'S':
                case 's': {
                    state = InputState::SECOND;
                    break;
                }
                default: {
                    bot_sendReplyMessage(bot, message,
                                         "Invalid value provided.\nShould contain only h, m, s, "
                                         "numbers, spaces. (ex. 1h 20m 7s)");

                    return;
                }
            }
        }
#ifndef NDEBUG
        PRETTYF("Date h %d m %d s %d", hms.h, hms.m, hms.s);
#endif
#define TIMER_CONFIG_SEC 5
        if (hms.toSeconds() == 0) {
            bot_sendReplyMessage(bot, message, "I'm not a fool to time 0s");
            return;
        } else if (hms.toSeconds() < TIMER_CONFIG_SEC) {
            bot_sendReplyMessage(bot, message, "Provide longer time value");
            return;
        }
        int msgid = bot.getApi()
                        .sendMessage(message->chat->id, "Timer starts")
                        ->messageId;
        bool couldpin = true;
        try {
            bot.getApi().pinChatMessage(message->chat->id, msgid);
        } catch (const std::exception &) {
            PRETTYF("Cannot pin msg!");
            couldpin = false;
        }
        using TimerType = std::remove_reference_t<decltype(*tm_ptr)>;
        tm_ptr = std::make_shared<TimerType>(TimerType(hms.h, hms.m, hms.s));
        tm_ptr->setCallback(
            [=](const TimerImpl_privdata *priv, struct timehms ms) {
                const Bot &bot = priv->bot;
                std::stringstream ss;
                if (ms.h != 0) ss << ms.h << "h ";
                if (ms.m != 0) ss << ms.m << "m ";
                if (ms.s != 0) ss << ms.s << "s ";
                if (!ss.str().empty() && ss.str() != message->text)
                    bot.getApi().editMessageText(ss.str(), message->chat->id,
                                                 priv->messageid);
            },
            TIMER_CONFIG_SEC,
            [=](const TimerImpl_privdata *priv) {
                const Bot &bot = priv->bot;
                bot.getApi().editMessageText("Timer ended", message->chat->id,
                                             priv->messageid);
                if (priv->sendendmsg)
                    bot.getApi().sendMessage(message->chat->id, "Timer ended");
                std::this_thread::sleep_for(std::chrono::seconds(1));
                if (priv->botcanpin)
                    bot.getApi().unpinChatMessage(message->chat->id,
                                                  priv->messageid);
            },
            std::make_unique<TimerImpl_privdata>(TimerImpl_privdata{
                msgid, bot, couldpin, true, message->chat->id}));
        tm_ptr->start();
    });
    bot_AddCommandEnforced(gbot, "stoptimer", [](const Bot &bot, const Message::Ptr &message) {
        bool ret = false;
        const char *text;
        if (tm_ptr) {
            ret = tm_ptr->cancel([&](TimerImpl_privdata *t) -> bool {
                bool allowed = t->chatid == message->chat->id;
                if (allowed) t->sendendmsg = false;
                return allowed;
            });
            if (ret) {
                text = "Stopped successfully";
            } else
                text = "Timer is running on other group.";
        } else
            text = "Timer is not running";
        bot_sendReplyMessage(bot, message, text);
    });
    bot_AddCommandPermissive(gbot, "decho", [](const Bot &bot, const Message::Ptr &message) {
        bool invalid = !hasExtArgs(message), sticker = false, text = false,
             animation = false;
        const auto msg = message->replyToMessage;

        if (msg) {
            if (msg->sticker)
                sticker = true;
            else if (msg->animation)
                animation = true;
            else if (!msg->text.empty())
                text = true;
        }
        try {
            bot.getApi().deleteMessage(message->chat->id, message->messageId);
            if (sticker && invalid) {
                bot.getApi().sendSticker(
                    message->chat->id, message->replyToMessage->sticker->fileId);
            } else if (animation && invalid) {
                bot.getApi().sendAnimation(
                    message->chat->id,
                    message->replyToMessage->animation->fileId);
            } else if (text && invalid) {
                bot_sendReplyMessage(bot, message, message->replyToMessage->text,
                                     message->replyToMessage->messageId);
            } else if (!invalid) {
                std::string msg;
                parseExtArgs(message, msg);
                bot_sendReplyMessage(bot, message, msg,
                                     (message->replyToMessage) ? message->replyToMessage->messageId : 0,
                                     true);
            }
        } catch (const std::exception &) {
            // bot is not adm. nothing it can do
        }
    });
    bot_AddCommandPermissive(gbot, "randsticker", [](const Bot &bot, const Message::Ptr &message) {
        if (message->replyToMessage && message->replyToMessage->sticker) {
            StickerSet::Ptr stickset;
            try {
                stickset = bot.getApi().getStickerSet(
                    message->replyToMessage->sticker->setName);
            } catch (const std::exception &e) {
                bot_sendReplyMessage(bot, message, e.what());
                return;
            }
            ssize_t pos = genRandomNumber(stickset->stickers.size() - 1);
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
    gbot.getEvents().onAnyMessage([&gbot](const Message::Ptr &message) {
        using UserId = int64_t;
        using ChatId = int64_t;
        using ChatHandle = std::map<UserId, std::vector<Message::Ptr>>;
        using UserType = std::pair<ChatId, std::vector<Message::Ptr>>;
        static std::map<ChatId, ChatHandle> buffer;
        static std::map<ChatId, int> buffer_sub;
        static std::mutex m;  // Protect buffer, buffer_sub
        static auto spamDetectFunc = [](decltype(buffer)::const_iterator handle) {
            std::map<UserId, size_t> MaxSameMsgMap, MaxMsgMap;
            // By most msgs sent by that user
            for (const auto &perUser : handle->second) {
                std::apply([&](const auto &first, const auto &second) {
                    MaxMsgMap.emplace(first, second.size());
#ifndef NDEBUG
                    PRETTYF("Chat: %ld, User: %ld, msgcnt: %ld", handle->first,
                            first, second.size());
#endif
                }, perUser);
            }

            // By most common msgs
            static auto commonMsgdataFn = [](const Message::Ptr &m) {
                if (m->sticker)
                    return m->sticker->fileUniqueId;
                else if (m->animation)
                    return m->animation->fileUniqueId;
                else
                    return m->text;
            };
            for (const auto &pair : handle->second) {
                std::multimap<std::string, Message::Ptr> byMsgContent;
                for (const auto &obj : pair.second) {
                    byMsgContent.emplace(commonMsgdataFn(obj), obj);
                }
                std::unordered_map<std::string, size_t> commonMap;
                for (const auto &cnt : byMsgContent) {
                    ++commonMap[cnt.first];
                }
                // Find the most common value
                auto mostCommonIt = std::max_element(commonMap.begin(), commonMap.end(),
                                                     [](const auto &lhs, const auto &rhs) {
                                                         return lhs.second < rhs.second;
                                                     });
                MaxSameMsgMap.emplace(pair.first, mostCommonIt->second);
#ifndef NDEBUG
                PRETTYF("Chat: %ld, User: %ld, maxIt: %ld", handle->first,
                        pair.first, mostCommonIt->second);
#endif
            }
#ifdef NDEBUG
            try {
                for (const auto &msg : pr->second) {
                    bot.getApi().deleteMessage(msg->chat->id, msg->messageId);
                }
                auto msg = pr->second.front();
                // Initial set - all false set
                auto perms = std::make_shared<ChatPermissions>();
                bot.getApi().restrictChatMember(msg->chat->id, msg->from->id, perms, 5 * 60);
            } catch (const std::exception &e) {
                PRETTYF("Error deleting msg: %s", e.what());
            }
#endif
        };

        if (Authorized(message, true)) return;
        // Do not track older msgs and consider it as spam.
        if (std::time(0) - message->date > 15) return;
        // We care GIF, sticker, text spams only
        if (!message->animation && message->text.empty() && !message->sticker)
            return;

        static std::once_flag once;
        std::call_once(once, [] {
            std::thread([] {
                while (true) {
                    {
                        const std::lock_guard<std::mutex> _(m);
                        auto its = buffer_sub.begin();
                        while (its != buffer_sub.end()) {
                            decltype(buffer)::const_iterator it = buffer.find(its->first);
                            if (it == buffer.end()) {
                                its = buffer_sub.erase(its);
                                continue;
                            }
#ifndef NDEBUG
                            PRETTYF("Chat: %ld, Count: %d", its->first, its->second);
#endif
                            if (its->second >= 5) {
#ifndef NDEBUG
                                PRETTYF("Launching spamdetect for %ld", its->first);
#endif
                                spamDetectFunc(it);
                            }
                            buffer.erase(it);
                            its->second = 0;
			    ++its;
                        }
                    }
                    std::this_thread::sleep_for(5s);
                }
            }).detach();
        });
        {
            const std::lock_guard<std::mutex> _(m);
            buffer[message->chat->id][message->from->id].emplace_back(message);
            ++buffer_sub[message->chat->id];
        }
    });

    static bool exited = false;
    static auto cleanupFunc = [](int s) {
        if (exited) return;
        PRETTYF("Exiting with signal %d", s);
        if (tm_ptr && tm_ptr->isrunning()) {
            tm_ptr->cancel();
            std::this_thread::sleep_for(std::chrono::seconds(4));
        }
        exited = true;
        std::exit(0);
    };
    static auto cleanupVoidFunc = [] { cleanupFunc(0); };

    std::signal(SIGINT, cleanupFunc);
    std::signal(SIGTERM, cleanupFunc);
    std::atexit(cleanupVoidFunc);
#ifdef __WIN32
    installHandler(cleanupVoidFunc);
#endif
    int64_t lastcrash = 0;

    PRETTYF("Debug: Token: %s", token.c_str());
reinit:
    try {
        PRETTYF("Debug: Bot username: %s", gbot.getApi().getMe()->username.c_str());
        gbot.getApi().deleteWebhook();

        TgLongPoll longPoll(gbot);
        while (true) {
            longPoll.start();
        }
    } catch (const std::exception &e) {
        PRETTYF("Error: %s", e.what());
        PRETTYF("Warning: Trying to recover");
#ifdef USE_DATABASE
        int64_t ownerid = database::db.maybeGetOwnerId().value_or(-1);
#endif
        try {
            gbot.getApi().sendMessage(ownerid, e.what());
        } catch (const std::exception &e) {
            PRETTYF("Critical Error: %s", e.what());
            return EXIT_FAILURE;
        }
        int64_t temptime = time(0);
        if (temptime - lastcrash < 10 && lastcrash != 0) {
            gbot.getApi().sendMessage(ownerid, "Recover failed.");
            PRETTYF("Error: Recover failed");
            return 1;
        }
        lastcrash = temptime;
        gbot.getApi().sendMessage(ownerid, "Reinitializing.");
        PRETTYF("Info: Re-init");
        gAuthorized = false;
        std::thread([] {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            gAuthorized = true;
        }).detach();
        goto reinit;
    }
}
