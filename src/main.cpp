#include <fcntl.h>
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

#define BOOST_BIND_GLOBAL_PLACEHOLDERS
#include <tgbot/tgbot.h>

#ifdef USE_BLACKLIST
#include "conf/conf.h"
#endif
#include "popen_wdt/popen_wdt.h"
#include "timer/Timer.h"

struct TimerImpl_privdata {
    int32_t messageid;
    const TgBot::Bot &bot;
    bool botcanpin, sendendmsg;
    int64_t chatid;
};

using TgBot::Bot;
using TgBot::Message;
using TgBot::MessageEntity;
using TgBot::StickerSet;
using TgBot::TgLongPoll;

static bool gAuthorized = true;

#ifdef USE_BLACKLIST
static TgBotConfig config("tgbot.dat");
#else
static const int64_t ownerid = 1185607882;
#endif

static inline void bot_sendReplyMessage(const Bot &bot, const Message::Ptr &message,
                                        const std::string &text, const int32_t replyToMsg = 0) {
    bot.getApi().sendMessage(message->chat->id, text,
                             true, (replyToMsg == 0) ? message->messageId : replyToMsg,
                             nullptr, "", false, std::vector<MessageEntity::Ptr>(), true);
}

static bool AuthorizedId(const int64_t id, const bool permissive) {
#ifdef USE_BLACKLIST
    static struct config_data data;
    config.loadFromFile(&data);
    if (!permissive) {
        return id == data.owner_id;
    } else {
        for (int i = 0; i < BLACKLIST_BUFFER; ++i) {
            if (data.blacklist[i] == id) return false;
        }
        return true;
    }
#else
    return permissive ? true : ownerid == id;
#endif
}

static inline bool Authorized(const Message::Ptr &message,
                              const bool nonuserallowed = false,
                              const bool permissive = false) {
    if (!gAuthorized) return false;
    return message->from ? AuthorizedId(message->from->id, permissive)
                         : nonuserallowed;
}

#define ENFORCE_AUTHORIZED \
    if (!Authorized(message)) return
#define PERMISSIVE_AUTHORIZED \
    if (!Authorized(message, true, true)) return

constexpr const char *STDERRTOOUT = "2>&1";
constexpr const int BUFSIZE = 1024;
constexpr const char *EMPTY = "<empty>";
constexpr char SPACE = ' ';

#define FILLIN_SENDWOERROR \
    nullptr, "", false, std::vector<MessageEntity::Ptr>(), true

static char *kCompiler = nullptr, *kCxxCompiler = nullptr;

static bool verifyMessage(const Bot &bot, const Message::Ptr &message) {
    ENFORCE_AUTHORIZED false;
    if (message->replyToMessage == nullptr) {
        bot_sendReplyMessage(bot, message, "Reply to a code to compile");
        return false;
    }
    return true;
}

static inline bool hasExtArgs(const Message::Ptr &message) {
    return message->text.find_first_of(" \n") != std::string::npos;
}

static void parseExtArgs(const Message::Ptr &message, std::string &extraargs) {
    // Telegram ensures message does not have whitespaces beginning or ending.
    auto id = message->text.find_first_of(" \n");
    if (id != std::string::npos) {
        extraargs = message->text.substr(id + 1);
    }
}

static bool writeMessageToFile(const Bot &bot, const Message::Ptr &message,
                               const char *filename) {
    std::ofstream file(filename);
    if (file.fail()) {
        bot_sendReplyMessage(bot, message, "Failed to open file");
        return false;
    }
    file << message->replyToMessage->text;
    file.close();
    return true;
}

static void addExtArgs(std::stringstream &cmd, std::string &extraargs,
                       std::string &res) {
    if (!extraargs.empty()) {
        auto idx = extraargs.find_first_of("\n\r\t");
        if (idx != std::string::npos) extraargs.erase(idx);
        cmd << SPACE << extraargs;
        res += "cmd: \"";
        res += cmd.str();
        res += "\"\n";
    }
    cmd << SPACE << STDERRTOOUT;
}

static void runCommand(const Bot &bot, const Message::Ptr &message,
                       const std::string &cmd, std::string &res) {
    bool fine = false;
    auto buf = std::make_unique<char[]>(BUFSIZE);
    int pipefd[2] = {-1, -1};

    pipe(pipefd);
    auto start = std::chrono::high_resolution_clock::now();
    auto fp = popen_watchdog(cmd.c_str(), pipefd[1]);

    if (!fp) {
        bot_sendReplyMessage(bot, message, "Failed to popen()");
        return;
    }
    while (fgets(buf.get(), BUFSIZE, fp)) {
        if (!fine) fine = true;
        res += buf.get();
    }
    if (!fine) res += std::string() + EMPTY + "\n";
    if (pipefd[0] != -1) {
        bool buf = false;
        int flags;
	using std::chrono::duration_cast;
	using std::chrono::seconds;
	using std::chrono::milliseconds;

        auto end = std::chrono::high_resolution_clock::now();
        if (duration_cast<seconds>(end - start).count() < SLEEP_SECONDS) {
            // Disable blocking (wdt would be sleeping if we are exiting earlier)
            flags = fcntl(pipefd[0], F_GETFL);
            fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);
        }

        read(pipefd[0], &buf, 1);
        if (buf) {
            res += WDT_BITE_STR;
        } else {
            float millis = static_cast<float>(duration_cast<milliseconds>(end - start).count());
            res += "-> It took " + std::to_string(millis * 0.001) + " seconds";
        }
        close(pipefd[0]);
        close(pipefd[1]);
    }
    pclose(fp);
}

static void commonCleanup(const Bot &bot, const Message::Ptr &message,
                          std::string &res, const char *filename) {
    if (res.size() > 4095) res.resize(4095);
    bot_sendReplyMessage(bot, message, res);
    if (filename) std::remove(filename);
}

static bool commonVerifyParseWrite(const Bot &bot, const Message::Ptr &message,
                                   std::string &extraargs, const char *filename) {
    bool ret = verifyMessage(bot, message);
    if (ret) {
        parseExtArgs(message, extraargs);
        ret = writeMessageToFile(bot, message, filename);
    }
    return ret;
}

static void CCppCompileHandler(const Bot &bot, const Message::Ptr &message,
                               const bool plusplus) {
    std::string res, extraargs;
    std::stringstream cmd, cmd2;
    const char *filename, aoutname[] = "./a.out";
    bool ret;

    if (plusplus) {
        filename = "./compile.cpp";
    } else {
        filename = "./compile.c";
    }

    if (!commonVerifyParseWrite(bot, message, extraargs, filename)) return;

    if (plusplus) {
        cmd << kCxxCompiler;
    } else {
        cmd << kCompiler;
    }
    cmd << SPACE << filename;
    addExtArgs(cmd, extraargs, res);
#ifdef DEBUG
    printf("cmd: %s\n", cmd.str().c_str());
#endif
    res += "Compile time:\n";
    runCommand(bot, message, cmd.str(), res);
    res += "\n";

    std::ifstream aout(aoutname);
    if (aout.good()) {
        aout.close();
        cmd.swap(cmd2);
        cmd << aoutname << SPACE << STDERRTOOUT;
        res += "Run time:\n";
        runCommand(bot, message, cmd.str(), res);
        std::remove(aoutname);
    }

    commonCleanup(bot, message, res, filename);
}

static void GenericRunHandler(const Bot &bot, const Message::Ptr &message,
                              const char *cmdPrefix, const char *outfile) {
    std::string res, extargs;
    std::stringstream cmd;
    bool ret;

    if (!commonVerifyParseWrite(bot, message, extargs, outfile)) return;

    cmd << cmdPrefix << SPACE;
    cmd << outfile;
    addExtArgs(cmd, extargs, res);

#ifdef DEBUG
    printf("cmd: %s\n", cmd.str().c_str());
#endif
    runCommand(bot, message, cmd.str(), res);
    commonCleanup(bot, message, res, outfile);
}

#define ARRAY_SIZE(arr) sizeof(arr) / sizeof(arr[0])

static void findCompiler(void) {
    static const char *const compilers[][2] = {
        {"clang", "clang++"},
        {"gcc", "g++"},
        {"cc", "c++"},
    };
    static char buffer[20];
    for (int i = 0; i < ARRAY_SIZE(compilers); i++) {
        auto checkfn = [i](const int idx) -> bool {
            memset(buffer, 0, sizeof(buffer));
            auto bytes = snprintf(buffer, sizeof(buffer), "/usr/bin/%s",
                                  compilers[i][idx]);
            if (bytes >= sizeof(buffer))
                return false;
            else
                buffer[bytes] = '\0';
            return access(buffer, R_OK | X_OK) == 0;
        };
        if (!kCompiler && checkfn(0)) {
            kCompiler = strdup(buffer);
        }
        if (!kCxxCompiler && checkfn(1)) {
            kCxxCompiler = strdup(buffer);
        }
        if (kCompiler && kCxxCompiler) break;
    }
}

static int genRandomNumber(const int upper, const int lower = 0) {
    if (upper == lower) return lower;
    std::random_device rd;
    std::mt19937 gen(rd());

    std::uniform_int_distribution<int> distribution(lower, upper);
    return distribution(gen);
}

#define CMD_UNSUPPORTED(cmd, reason)                                     \
    bot.getEvents().onCommand(cmd, [&bot](const Message::Ptr &message) { \
        bot_sendReplyMessage(                                            \
            bot, message,                                                \
            "cmd '" cmd "' is unsupported.\nReason: " reason);           \
    });

int main(void) {
    const char *token_str = getenv("TOKEN");
    if (!token_str) {
        printf("TOKEN is not exported\n");
        return 1;
    }
    std::string token(token_str);
    printf("Token: %s\n", token.c_str());

    Bot bot(token);
    static std::shared_ptr<Timer<TimerImpl_privdata>> tm_ptr;
    findCompiler();

    if (kCxxCompiler) {
        bot.getEvents().onCommand("cpp", [&bot](const Message::Ptr &message) {
            CCppCompileHandler(bot, message, true);
        });
    } else {
        CMD_UNSUPPORTED("cpp", "Host does not have a C++ compiler");
    }
    if (kCompiler) {
        bot.getEvents().onCommand("c", [&bot](const Message::Ptr &message) {
            CCppCompileHandler(bot, message, false);
        });
    } else {
        CMD_UNSUPPORTED("c", "Host does not have a C compiler");
    }
    bot.getEvents().onCommand("python", [&bot](const Message::Ptr &message) {
        GenericRunHandler(bot, message, "python3", "./out.py");
    });
    if (access("/usr/bin/go", F_OK) == 0) {
        bot.getEvents().onCommand("golang", [&bot](const Message::Ptr &message) {
            GenericRunHandler(bot, message, "go run", "./out.go");
        });
    } else {
        CMD_UNSUPPORTED("golang", "Host does not have a Go compiler");
    }
#ifdef USE_BLACKLIST
    bot.getEvents().onCommand("addblacklist", [&bot](const Message::Ptr &message) {
        ENFORCE_AUTHORIZED;
        struct config_data data;
        if (message->replyToMessage && message->replyToMessage->from) {
            config.loadFromFile(&data);
            if (data.owner_id == message->replyToMessage->from->id) {
                bot_sendReplyMessage(bot, message,
                                     "Why would I blacklist my owner? Looks like a pretty bad idea.");
                return;
            }
            for (int i = 0; i < BLACKLIST_BUFFER; i++) {
                if (data.blacklist[i] == message->replyToMessage->from->id) {
                    bot_sendReplyMessage(bot, message, "User already in blacklist");
                    return;
                }
                if (data.blacklist[i] == 0) {
                    data.blacklist[i] = message->replyToMessage->from->id;
                    bot_sendReplyMessage(bot, message, "User added to blacklist");
                    config.storeToFile(data);
                    return;
                }
            }
            bot_sendReplyMessage(bot, message, "Out of buffer");
        } else {
            bot_sendReplyMessage(bot, message, "Reply to a user");
        }
    });
    bot.getEvents().onCommand("rmblacklist", [&bot](const Message::Ptr &message) {
        ENFORCE_AUTHORIZED;
        struct config_data data;
        if (message->replyToMessage && message->replyToMessage->from) {
            config.loadFromFile(&data);
            int tmp[BLACKLIST_BUFFER] = {
                0,
            };
            for (int i = 0; i < BLACKLIST_BUFFER; i++) {
                if (data.blacklist[i] == message->replyToMessage->from->id) {
                    bot_sendReplyMessage(bot, message, "User removed from blacklist");
                    continue;
                } else {
                    tmp[i] = data.blacklist[i];
                }
            }
            memcpy(data.blacklist, tmp, sizeof(tmp));
            config.storeToFile(data);
        } else {
            bot_sendReplyMessage(bot, message, "Reply to a user");
        }
    });
#else
    CMD_UNSUPPORTED("addblacklist", "USE_BLACKLIST flag not enabled");
    CMD_UNSUPPORTED("rmblacklist", "USE_BLACKLIST flag not enabled");
#endif
    bot.getEvents().onCommand("bash", [&bot](const Message::Ptr &message) {
        std::string res;
        ENFORCE_AUTHORIZED;
        if (hasExtArgs(message)) {
            std::string cmd;
            parseExtArgs(message, cmd);
            runCommand(bot, message, cmd + SPACE + STDERRTOOUT, res);
        } else {
            res = "Send a bash command to run";
        }
        commonCleanup(bot, message, res, nullptr);
    });

    bot.getEvents().onCommand("alive", [&bot](const Message::Ptr &message) {
        PERMISSIVE_AUTHORIZED;
        static int64_t lasttime = 0, time = 0;
        time = std::time(0);
        if (lasttime != 0 && time - lasttime < 5) return;
        lasttime = time;
#if defined(GIT_COMMITID) && defined(GIT_COMMITMSG)
#define VERSION_STR "HEAD Commit-Id: " GIT_COMMITID "\n" "HEAD Commit-Msg: \"" GIT_COMMITMSG "\""
#else
#define VERSION_STR ""
#endif
        // Hardcoded bocchi the rock GIF
        bot.getApi().sendAnimation(message->chat->id,
                                   "CgACAgQAAx0CVJDJywAChOJkqVx5kGpydVpJGpqXp6E"
                                   "khb3IzAACxAMAAqDqHVM3s6FG-iRuoC8E",
                                   0, 0, 0, "", VERSION_STR, message->messageId,
                                   FILLIN_SENDWOERROR, false, 0, false);
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
                t->sendendmsg = false;
                return t->chatid == message->chat->id;
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
                    message->chat->id, message->replyToMessage->sticker->fileId,
                    0, nullptr, false, true);
            } else if (animation && invalid) {
                bot.getApi().sendAnimation(
                    message->chat->id,
                    message->replyToMessage->animation->fileId, 0, 0, 0, "", "",
                    0, FILLIN_SENDWOERROR, false, 0, false);
            } else if (text && invalid) {
                bot_sendReplyMessage(bot, message, message->replyToMessage->text,
                                     message->replyToMessage->messageId);
            } else if (!invalid) {
                std::string msg;
                parseExtArgs(message, msg);
                bot_sendReplyMessage(bot, message, msg, (message->replyToMessage) ? message->replyToMessage->messageId : 0);
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
#ifdef DEBUG
                        bot.getApi().sendMessage(chatid,
                                                 "User:" + tag + " Count: " + std::to_string(pr->second));
#else
                        // clang-format off
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
                        // clang-format on
#endif
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

    static auto cleanupFunc = [](int s) {
        if (kCompiler) free(kCompiler);
        if (kCxxCompiler) free(kCxxCompiler);
        printf("Exiting with signal %d\n", s);
        if (tm_ptr) {
            tm_ptr->cancel();
            std::this_thread::sleep_for(std::chrono::seconds(4));
        }
        exit(0);
    };
    auto cleanupVoidFunc = [] { cleanupFunc(0); };

    std::signal(SIGINT, cleanupFunc);
    std::signal(SIGTERM, cleanupFunc);
    std::atexit(cleanupVoidFunc);
    int64_t lastcrash = 0;

#define PRETTYF(fmt, ...) printf("[%s:%d] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)

reinit:
    try {
        PRETTYF("Debug: Bot username: %s", bot.getApi().getMe()->username.c_str());
        bot.getApi().deleteWebhook();

        TgLongPoll longPoll(bot);
        while (true) {
            longPoll.start();
        }
    } catch (std::exception &e) {
        PRETTYF("Error: %s", e.what());
        PRETTYF("Warning: Trying to recover");
#ifdef USE_BLACKLIST
        static struct config_data data;
        config.loadFromFile(&data);
        int64_t ownerid = data.owner_id;
#endif
        try {
            bot.getApi().sendMessage(ownerid, e.what());
        } catch (const TgBot::TgException &e) {
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
