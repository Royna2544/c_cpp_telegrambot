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

#include <Authorization.h>
#include <BotReplyMessage.h>
#include <CompilerInTelegram.h>
#include <NamespaceImport.h>

#include "timer/Timer.h"
#include "utils/libutils.h"

struct TimerImpl_privdata {
    int32_t messageid;
    const TgBot::Bot &bot;
    bool botcanpin, sendendmsg;
    int64_t chatid;
};

using TgBot::MessageEntity;
using TgBot::StickerSet;
using TgBot::TgLongPoll;

static char *kCompiler = nullptr, *kCxxCompiler = nullptr;

#define CMD_UNSUPPORTED(cmd, reason)                                     \
    bot.getEvents().onCommand(cmd, [&bot](const Message::Ptr &message) { \
        bot_sendReplyMessage(                                            \
            bot, message,                                                \
            "cmd '" cmd "' is unsupported.\nReason: " reason);           \
    });

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
    Bot bot(token);
    static std::shared_ptr<Timer<TimerImpl_privdata>> tm_ptr;
    findCompiler(&kCompiler, &kCxxCompiler);

    if (kCxxCompiler) {
        bot.getEvents().onCommand("cpp", [&bot](const Message::Ptr &message) {
            CompileRunHandler<CCppCompileHandleData>({bot, message, kCxxCompiler, "compile.cpp"});
        });
    } else {
        CMD_UNSUPPORTED("cpp", "Host does not have a C++ compiler");
    }
    if (kCompiler) {
        bot.getEvents().onCommand("c", [&bot](const Message::Ptr &message) {
            CompileRunHandler<CCppCompileHandleData>({bot, message, kCompiler, "compile.c"});
        });
    } else {
        CMD_UNSUPPORTED("c", "Host does not have a C compiler");
    }
    bot.getEvents().onCommand("python", [&bot](const Message::Ptr &message) {
        CompileRunHandler({bot, message, "python3", "./out.py"});
    });
    if (access("/usr/bin/go", F_OK | X_OK) == 0) {
        bot.getEvents().onCommand("golang", [&bot](const Message::Ptr &message) {
            CompileRunHandler({bot, message, "go run", "./out.go"});
        });
    } else {
        CMD_UNSUPPORTED("golang", "Host does not have a Go compiler");
    }
#ifdef USE_DATABASE
    bot.getEvents().onCommand("addblacklist", [&bot](const Message::Ptr &message) {
        ENFORCE_AUTHORIZED;
        database::blacklist.addToDatabase(bot, message);
    });
    bot.getEvents().onCommand("rmblacklist", [&bot](const Message::Ptr &message) {
        ENFORCE_AUTHORIZED;
        database::blacklist.removeFromDatabase(bot, message);
    });
    bot.getEvents().onCommand("addwhitelist", [&bot](const Message::Ptr &message) {
        ENFORCE_AUTHORIZED;
        database::whitelist.addToDatabase(bot, message);
    });
    bot.getEvents().onCommand("rmwhitelist", [&bot](const Message::Ptr &message) {
        ENFORCE_AUTHORIZED;
        database::whitelist.removeFromDatabase(bot, message);
    });
    bot.getEvents().onCommand("savedb", [&bot](const Message::Ptr &message) {
        ENFORCE_AUTHORIZED;
        database::db.save();
    });
#else
#define NOT_SUPPORTED_DB(name) CMD_UNSUPPORTED(name, "USE_DATABASE flag not enabled");
    NOT_SUPPORTED_DB("addblacklist");
    NOT_SUPPORTED_DB("rmblacklist");
    NOT_SUPPORTED_DB("addwhitelist");
    NOT_SUPPORTED_DB("rmwhitelist");
#undef NOT_SUPPORTED_DB
#endif
    bot.getEvents().onCommand("bash", [&bot](const Message::Ptr &message) {
        CompileRunHandler(BashHandleData{bot, message, false});
    });
    bot.getEvents().onCommand("unsafebash", [&bot](const Message::Ptr &message) {
        CompileRunHandler(BashHandleData{bot, message, true});
    });
    bot.getEvents().onCommand("alive", [&bot](const Message::Ptr &message) {
        PERMISSIVE_AUTHORIZED;
        static int64_t lasttime = 0, time = 0;
        time = std::time(0);
        if (lasttime != 0 && time - lasttime < 5) return;
        lasttime = time;
#if defined(GIT_COMMITID) && defined(GIT_COMMITMSG)
#ifdef GIT_ORIGIN_URL
#define BUILD_URL_STR "Commit-Id: <a href=\"" GIT_ORIGIN_URL "/commit/" GIT_COMMITID "\">" GIT_COMMITID \
                      "</a>"                                                                            \
                      "\nRepo URL: <a href=\"" GIT_ORIGIN_URL "\">Here</a>"
#else
#define BUILD_URL_STR "Commit-Id: " GIT_COMMITID
#endif
#define VERSION_STR                                                                  \
    "<b>ABOUT</b>:\n- Telegram bot for fun\n"                                        \
    "- Proudly backed by C/C++.\n- Supports in-chat compiler support (Owner only)\n" \
    "- Awesome spam purge feature\n- Utilites support\n\n"                           \
    "<b>BUILD INFO</b>: HEAD\nCommit-Msg: \"" GIT_COMMITMSG "\"\n" BUILD_URL_STR
#else
#define VERSION_STR ""
#endif
        // Hardcoded Cum about it GIF
        bot.getApi().sendAnimation(message->chat->id,
                                   "CgACAgQAAx0CdY7-CgABARtpZLefgyKNpSLvyCJWcp8"
                                   "mt5KF_REAAgkDAAI2tFRQIk0uTVxfZnsvBA",
                                   0, 0, 0, "", VERSION_STR, message->messageId,
                                   nullptr, "html");
    });
    bot.getEvents().onCommand("flash", [&bot](const Message::Ptr &message) {
        PERMISSIVE_AUTHORIZED;
        // static const std::vector<std::string> reasons; in "FlashData.h"
#include "FlashData.h"
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
    bot.getEvents().onCommand("possibility", [&bot](const Message::Ptr &message) {
        PERMISSIVE_AUTHORIZED;
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
    bot.getEvents().onCommand("starttimer", [&bot](const Message::Ptr &message) {
        enum InputState {
            HOUR,
            MINUTE,
            SECOND,
            NONE,
        };
        ENFORCE_AUTHORIZED;
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
#ifdef DEBUG
                        printf("result: %d\n", result);
#endif
                        state = InputState::NONE;
                        numbercache.clear();
                    }
                    break;
                }
                case '0' ... '9': {
                    int intver = code - 48;
#ifdef DEBUG
                    printf("%d\n", intver);
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
#ifdef DEBUG
        printf("Date h %d m %d s %d\n", hms.h, hms.m, hms.s);
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
            printf("Cannot pin msg!\n");
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
    bot.getEvents().onCommand("stoptimer", [&bot](const Message::Ptr &message) {
        ENFORCE_AUTHORIZED;
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
    bot.getEvents().onCommand("decho", [&bot](const Message::Ptr &message) {
        PERMISSIVE_AUTHORIZED;
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
    bot.getEvents().onCommand("randsticker", [&bot](const Message::Ptr &message) {
        PERMISSIVE_AUTHORIZED;
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
    bot.getEvents().onAnyMessage([&bot](const Message::Ptr &message) {
        static std::vector<Message::Ptr> buffer;
        static std::mutex m;  // Protect buffer
        static std::once_flag once;

        if (Authorized(message, true)) return;
        // Do not track older msgs and consider it as spam.
        if (std::time(0) - message->date > 15) return;
        // We care GIF, sticker, text spams only
        if (!message->animation && message->text.empty() && !message->sticker)
            return;

        std::call_once(once, [&] {
            std::thread([&]() {
                while (true) {
                    auto t = std::thread([&]() {
                        std::string tag;
                        std::vector<Message::Ptr> buffer_priv;
                        std::map<int64_t, int> spammap, chatidmap;
                        int64_t chatid = -1;
                        int maxvalue = 0;
                        {
                            const std::lock_guard<std::mutex> _(m);
                            buffer_priv = buffer;
                            buffer.clear();
                        }
#ifdef DEBUG
                        printf("Buffer size: %lu\n", buffer_priv.size());
#endif
                        if (buffer_priv.size() <= 1) return;
                        for (const auto &msg : buffer_priv) {
                            chatidmap[msg->chat->id]++;
                        }

                        for (const auto &pair : chatidmap) {
                            if (pair.second > maxvalue) {
                                chatid = pair.first;
                                maxvalue = pair.second;
                            }
                        }
                        buffer_priv.erase(std::remove_if(buffer_priv.begin(), buffer_priv.end(),
                                                         [&](const Message::Ptr &s) {
                                                             return s->chat->id != chatid;
                                                         }),
                                          buffer_priv.end());

                        for (const auto &msg : buffer_priv) {
                            if (msg->from)
                                spammap[msg->from->id]++;
                        }
                        using pair_type = decltype(spammap)::value_type;
                        auto pr = std::max_element(
                            std::begin(spammap), std::end(spammap),
                            [](const pair_type &p1, const pair_type &p2) {
                                return p1.second > p2.second;
                            });
                        // Reasonable value 5 from experiments
                        if (pr->second <= 5) return;
                        for (const auto &msg : buffer_priv) {
                            if (msg->from && msg->from->id == pr->first)
                                if (!msg->from->username.empty()) tag = " @" + msg->from->username;
                        }
                        bot.getApi().sendMessage(chatid, "Spam detected" + tag);
                        try {
                            for (const auto &msg : buffer_priv) {
                                if (msg->from && msg->from->id == pr->first) {
                                    bot.getApi().deleteMessage(msg->chat->id, msg->messageId);
                                }
                            }
                        } catch (const std::exception &) {
                            printf("Error deleting msg\n");
                        }
                    });
                    t.detach();
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                }
            }).detach();
        });
        {
            const std::lock_guard<std::mutex> _(m);
            buffer.push_back(message);
        }
    });

    static bool exited = false;
    static auto cleanupFunc = [](int s) {
        if (exited) return;
        if (kCompiler) free(kCompiler);
        if (kCxxCompiler) free(kCxxCompiler);
        printf("Exiting with signal %d\n", s);
        if (tm_ptr && tm_ptr->isrunning()) {
            tm_ptr->cancel();
            std::this_thread::sleep_for(std::chrono::seconds(4));
        }
        exited = true;
        std::exit(0);
    };
    auto cleanupVoidFunc = [] { cleanupFunc(0); };

    std::signal(SIGINT, cleanupFunc);
    std::signal(SIGTERM, cleanupFunc);
    std::atexit(cleanupVoidFunc);
    int64_t lastcrash = 0;

    PRETTYF("Debug: Token: %s", token.c_str());
reinit:
    try {
        PRETTYF("Debug: Bot username: %s", bot.getApi().getMe()->username.c_str());
        bot.getApi().deleteWebhook();

        TgLongPoll longPoll(bot);
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
            bot.getApi().sendMessage(ownerid, e.what());
        } catch (const std::exception &e) {
            PRETTYF("Critical Error: %s", e.what());
            return EXIT_FAILURE;
        }
        int64_t temptime = time(0);
        if (temptime - lastcrash < 10 && lastcrash != 0) {
            bot.getApi().sendMessage(ownerid, "Recover failed.");
            PRETTYF("Error: Recover failed");
            return 1;
        }
        lastcrash = temptime;
        bot.getApi().sendMessage(ownerid, "Reinitializing.");
        PRETTYF("Info: Re-init");
        gAuthorized = false;
        std::thread([] {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            gAuthorized = true;
        }).detach();
        goto reinit;
    }
}
