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
#include <memory>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

#define BOOST_BIND_GLOBAL_PLACEHOLDERS
#include <tgbot/tgbot.h>

#include "Timer.h"
#include "TimerImpl_privdata.h"

using namespace TgBot;

// #define DEBUG

static inline bool Authorized(const Message::Ptr &message) {
    return message->from ? message->from->id == 1185607882 : true;
}

#define FILENAME "./compile.cpp"
#define AOUTNAME "./a.out"
#define STDERRTOOUT "2>&1"
#define BUFSIZE 1024
#define EMPTY "<empty>"
#define SPACE " "

#define FILLIN_SENDWOERROR \
    nullptr, "", false, std::vector<MessageEntity::Ptr>(), true

static char *kCompiler = nullptr, *kCxxCompiler = nullptr;

static bool verifyMessage(const Bot &bot, const Message::Ptr &message) {
    if (!Authorized(message)) return false;
    if (message->replyToMessage == nullptr) {
        bot.getApi().sendMessage(message->chat->id,
                                 "Reply to a code to compile", false,
                                 message->messageId, FILLIN_SENDWOERROR);
        return false;
    }
    return true;
}

static inline bool hasExtArgs(const Message::Ptr &message) {
    return message->text.find_first_of(" ") != std::string::npos;
}

static void parseExtArgs(const Message::Ptr &message, std::string &extraargs) {
    // Telegram ensures message does not have whitespaces beginning or ending.
    auto id = message->text.find_first_of(" ");
    if (id != std::string::npos) {
        extraargs = message->text.substr(id + 1);
    }
}

static bool writeMessageToFile(const Bot &bot, const Message::Ptr &message,
                               const char *filename) {
    std::ofstream file(filename);
    if (file.fail()) {
        bot.getApi().sendMessage(message->chat->id, "Failed to open file",
                                 false, message->messageId, FILLIN_SENDWOERROR);
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
    auto fp = popen(cmd.c_str(), "r");
    if (!fp) {
        bot.getApi().sendMessage(message->chat->id, "Failed to popen()", false,
                                 message->messageId, FILLIN_SENDWOERROR);
        return;
    }
    while (fgets(buf.get(), BUFSIZE, fp)) {
        if (!fine) fine = true;
        res += buf.get();
    }
    pclose(fp);
    if (!fine) res += EMPTY;
}

static void commonCleanup(const Bot &bot, const Message::Ptr &message,
                          std::string &res, const char *filename) {
    if (res.size() > 4095) res.resize(4095);
    bot.getApi().sendMessage(message->chat->id, res.c_str(), false,
                             message->messageId, FILLIN_SENDWOERROR);
    if (filename) std::remove(filename);
}

static void CCppCompileHandler(const Bot &bot, const Message::Ptr &message,
                               const bool plusplus) {
    std::string res, extraargs;
    std::stringstream cmd, cmd2;
    bool ret;

    ret = verifyMessage(bot, message);
    if (!ret) return;
    parseExtArgs(message, extraargs);
    ret = writeMessageToFile(bot, message, FILENAME);
    if (!ret) return;

    if (plusplus) {
        cmd << kCxxCompiler;
    } else {
        cmd << kCompiler;
    }
    cmd << SPACE << "-x" << SPACE;
    if (plusplus) {
        cmd << "c++";
    } else {
        cmd << "c";
    }
    cmd << SPACE << FILENAME;
    addExtArgs(cmd, extraargs, res);
#ifdef DEBUG
    printf("cmd: %s\n", cmd.str().c_str());
#endif
    res += "Compile time:\n";
    runCommand(bot, message, cmd.str(), res);
    res += "\n";

    std::ifstream aout(AOUTNAME);
    if (!aout.good()) goto sendresult;
    cmd.swap(cmd2);
    cmd << AOUTNAME << SPACE << STDERRTOOUT;
    res += "Run time:\n";
    runCommand(bot, message, cmd.str(), res);

sendresult:
    commonCleanup(bot, message, res, FILENAME);
    std::remove(AOUTNAME);
}

static void GenericRunHandler(const Bot &bot, const Message::Ptr &message,
                              const char *cmdPrefix, const char *outfile) {
    std::string res, extargs;
    std::stringstream cmd;
    bool ret;

    ret = verifyMessage(bot, message);
    if (!ret) return;
    parseExtArgs(message, extargs);
    ret = writeMessageToFile(bot, message, outfile);
    if (!ret) return;

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
        bot.getApi().sendMessage(                                        \
            message->chat->id,                                           \
            "cmd '" cmd "' is unsupported.\nReason: " reason, false,     \
            message->messageId, FILLIN_SENDWOERROR);                     \
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
        bot.getEvents().onCommand(
            "golang", [&bot](const Message::Ptr &message) {
                GenericRunHandler(bot, message, "go run", "./out.go");
            });
    } else {
        CMD_UNSUPPORTED("golang", "Host does not have a Go compiler");
    }
    bot.getEvents().onCommand("bash", [&bot](const Message::Ptr &message) {
        std::string res;
        if (!Authorized(message)) return;
        if (hasExtArgs(message)) {
            std::string cmd;
            parseExtArgs(message, cmd);
            runCommand(bot, message, std::string() + cmd + SPACE STDERRTOOUT,
                       res);
        } else {
            res = "Send a bash command to run";
        }
        commonCleanup(bot, message, res, nullptr);
    });

    bot.getEvents().onCommand("alive", [&bot](const Message::Ptr &message) {
        static int64_t lasttime = 0, time = 0;
        time = std::time(0);
        if (lasttime != 0 && time - lasttime < 5) return;
        lasttime = time;
        bot.getApi().sendMessage(message->chat->id, "I am alive...", false,
                                 message->messageId, FILLIN_SENDWOERROR);
    });
    bot.getEvents().onCommand("flash", [&bot](const Message::Ptr &message) {
        static const std::vector<std::string> reasons = {
            "Alex is not sleeping",
            "The system has been destoryed",
            "init: service 'recovery' got signal 6",
            "Rewrite the updater script with cpp and try again",
            "Incorrect bootloader version.",
            "Insufficient storage space to install the custom ROM.",
            "The installation process was interrupted due to a "
            "power failure.",
            "The file is not compatible with your device.",
            "The installation package is corrupt.",
            "Security boot error. UEFI boot failed.",
            "Device not rooted",
            "Installation failed due to a user error.",
            "The zip is incompatible with the device's kernel.",
            "Not enough space available in system partition.",
            "Incorrect MD5 checksum.",
            "Your bootloader is unlocked. Please lock the bootloader "
            "before installing.",
            "The device is not supported.",
            "The installation process was cancelled by the user.",
            "The file is encrypted or password protected.",
            "Requires a newer version of TWRP Recovery.",
            "Package signature verification failed.",
            "The installation process failed due to an unknown error.",
            "Not compatible with your device's screen resolution.",
            "Your device is incompatible with the build date fingerprint.",
            "The zip is missing important system files.",
            "Verification failed. BSOD is not enabled.",
            "File name does not match the expected format.",
            "Package is missing important system libraries or binaries.",
            "Installation process was interrupted due to a "
            "network error.",
            "The device is running an unsupported firmware version.",
            "The device is not rooted properly",
            "The file is incompatible with the device's recovery image.",
            "bootloop",
            "error 7",
            "zip corrupted",
            "battery died",
            "virus",
            "not enough space",
            "locked bootloader",
            "knox 0x0",
            "try again later",
            "yumi protection",
            "sdcard corrupted",
            "verizon locked carrier",
            "emmc died",
            "nusantara recovery not supported",
            "battery corruption",
            "failed to mount system",
            "failed to mount system_ext",
            "failed to mount product",
            "failed to mount vendor",
            "failed to mount partitions",
            "system is mounted as read-only",
            "system_ext is mounted as read-only",
            "product is mounted as read-only",
            "vendor is mounted as read-only",
            "beep bop... something wrong",
            "your opinion rejected",
            "Not installed because the device is "
            "currently encrypted."};

        std::string msg = message->text;
        if (message->replyToMessage != nullptr) {
            msg = message->replyToMessage->text;
            goto parse;
        }
        if (!hasExtArgs(message)) {
            bot.getApi().sendMessage(message->chat->id, "Send a file name",
                                     false, message->messageId,
                                     FILLIN_SENDWOERROR);
            return;
        }
        parseExtArgs(message, msg);
    parse:
        if (msg.empty()) {
            bot.getApi().sendMessage(message->chat->id, "Reply to a text",
                                     false, message->messageId,
                                     FILLIN_SENDWOERROR);
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
        bot.getApi().sendMessage(message->chat->id, ss.str(), false,
                                 message->messageId, FILLIN_SENDWOERROR);
    });
    bot.getEvents().onCommand(
        "possibility", [&bot](const Message::Ptr &message) {
            if (!hasExtArgs(message)) {
                bot.getApi().sendMessage(
                    message->chat->id,
                    "Send avaliable conditions sperated by newline", false,
                    message->messageId, FILLIN_SENDWOERROR);
                return;
            }
            std::string text;
            parseExtArgs(message, text);
            std::stringstream ss(text), out;
            std::string line, last;
            std::vector<std::string> vec;
            std::random_device rd;
            std::mt19937 gen(rd());

            int numlines = 1;
            for (char c : message->text) {
                if (c == '\n') numlines++;
            }
            vec.reserve(numlines);
            while (std::getline(ss, line, '\n')) {
                if (std::all_of(line.begin(), line.end(),
                                [](char c) { return std::isspace(c); }))
                    continue;
                vec.push_back(line);
            }
            if (vec.size() == 1) {
                bot.getApi().sendMessage(
                    message->chat->id, "Give more than 1 choice", false,
                    message->messageId, FILLIN_SENDWOERROR);
                return;
            }
            std::shuffle(vec.begin(), vec.end(), gen);
            out << "Total " << vec.size() << " items" << std::endl;
            last = vec.back();
            vec.pop_back();
            int total = 0;
            for (const auto &cond : vec) {
                int thisper = genRandomNumber(100 - total);
                out << cond << " : " << thisper << "%" << std::endl;
                total += thisper;
            }
            out << last << " : " << 100 - total << "%" << std::endl;
            bot.getApi().sendMessage(message->chat->id, out.str(), false,
                                     message->messageId, FILLIN_SENDWOERROR);
        });
    bot.getEvents().onCommand("shutdown", [&bot](const Message::Ptr &message) {
        if (Authorized(message) && std::time(0) - message->date < 5) exit(0);
    });
    bot.getEvents().onCommand("starttimer", [&bot](
                                                const Message::Ptr &message) {
        enum InputState {
            HOUR,
            MINUTE,
            SECOND,
            NONE,
        };
        if (!Authorized(message)) return;
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
            bot.getApi().sendMessage(
                message->chat->id, "Send or reply to a time, in hhmmss format",
                false, message->messageId, FILLIN_SENDWOERROR);
            return;
        }
        if (tm_ptr && tm_ptr->isrunning()) {
            bot.getApi().sendMessage(message->chat->id,
                                     "Timer is already running", false,
                                     message->messageId, FILLIN_SENDWOERROR);
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
                    bot.getApi().sendMessage(
                        message->chat->id,
                        "Invalid value provided.\nShould contain only h, m, s, "
                        "numbers, spaces. (ex. 1h 20m 7s)",
                        false, message->messageId, FILLIN_SENDWOERROR);

                    return;
                }
            }
        }
#ifdef DEBUG
        printf("Date h %d m %d s %d\n", hms.h, hms.m, hms.s);
#endif
#define TIMER_CONFIG_SEC 5
        if (hms.toSeconds() == 0) {
            bot.getApi().sendMessage(message->chat->id,
                                     "I'm not a fool to time 0s", false,
                                     message->messageId, FILLIN_SENDWOERROR);
            return;
        } else if (hms.toSeconds() < TIMER_CONFIG_SEC) {
            bot.getApi().sendMessage(message->chat->id,
                                     "Provide longer time value", false,
                                     message->messageId, FILLIN_SENDWOERROR);
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
        if (!Authorized(message)) return;
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
        bot.getApi().sendMessage(message->chat->id, text, false,
                                 message->messageId, FILLIN_SENDWOERROR);
    });
    bot.getEvents().onCommand("decho", [&bot](const Message::Ptr &message) {
        bool invalid = false, sticker = false, text = false, animation = false;
        if (!hasExtArgs(message)) invalid = true;
        if (const auto msg = message->replyToMessage; msg) {
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
                bot.getApi().sendMessage(
                    message->chat->id, message->replyToMessage->text, false,
                    message->replyToMessage->messageId, FILLIN_SENDWOERROR);
            } else if (!invalid) {
                std::string msg;
                parseExtArgs(message, msg);
                bot.getApi().sendMessage(
                    message->chat->id, msg, false,
                    (message->replyToMessage)
                        ? message->replyToMessage->messageId
                        : 0,
                    FILLIN_SENDWOERROR);
            }
        } catch (const std::exception &) {
            // bot is not adm. nothing it can do
        }
    });
    bot.getEvents().onCommand("randsticker", [&bot](
                                                 const Message::Ptr &message) {
        if (message->replyToMessage && message->replyToMessage->sticker) {
            StickerSet::Ptr stickset;
            try {
                stickset = bot.getApi().getStickerSet(
                    message->replyToMessage->sticker->setName);
            } catch (const std::exception &e) {
                bot.getApi().sendMessage(message->chat->id, e.what(), false,
                                         message->messageId,
                                         FILLIN_SENDWOERROR);
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
            bot.getApi().sendMessage(message->chat->id, ss.str(), false,
                                     message->messageId, FILLIN_SENDWOERROR);
        } else {
            bot.getApi().sendMessage(
                message->chat->id, "Sticker not found in replied-to message",
                false, message->messageId, FILLIN_SENDWOERROR);
        }
    });
    bot.getEvents().onAnyMessage([&bot](const Message::Ptr &message) {
        static std::vector<Message::Ptr> buffer;
        static std::mutex m;
        static std::atomic_bool cb;
        static bool falseth;

        if (Authorized(message)) return;
        // Do not track older msgs and consider it as spam.
        if (std::time(0) - message->date > 15) return;
        // We care GIF, sticker, text spams only
        if (!message->animation && message->text.empty() && !message->sticker)
            return;

        if (!falseth) {
            std::thread([cb = &cb]() {
                falseth = true;
                while (true) {
                    if (!cb) break;
                    *cb = false;
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                }
                falseth = false;
            }).detach();
        }
        {
            const std::lock_guard<std::mutex> _(m);
            buffer.push_back(message);
        }
        if (!cb) {
            cb = true;
            std::thread([&]() {
                struct simpleuser {
                    int64_t id;
                    std::string username;
                    int spamcnt;
                };
                bool spam = false;
                std::vector<struct simpleuser> spamvec;
                std::this_thread::sleep_for(std::chrono::seconds(5));
                const std::lock_guard<std::mutex> _(m);
#ifdef DEBUG
                printf("Buffer size: %lu\n", buffer.size());
#endif
                if (buffer.size() >= 5) {
                    int64_t chatid = buffer.front()->chat->id;
                    for (const auto &msg : buffer) {
                        if (msg->chat->id != chatid) continue;
                        bool found = false;
                        struct simpleuser *ptr;
                        for (const auto &user : spamvec) {
                            found = user.id == msg->from->id;
                            if (found) {
                                ptr = const_cast<struct simpleuser *>(&user);
                                break;
                            }
                        }
                        if (found) {
                            ptr->spamcnt += 1;
                        } else {
                            spamvec.push_back(
                                {msg->from->id, msg->from->username, 1});
                        }
                    }
                    spam = spamvec.size() < 3;
                }
                if (spam) {
                    using pair_type = decltype(spamvec)::value_type;
                    auto pr = std::max_element(
                        std::begin(spamvec), std::end(spamvec),
                        [](const pair_type &p1, const pair_type &p2) {
                            return p1.spamcnt < p2.spamcnt;
                        });
                    std::string tag;
                    if (!pr->username.empty()) tag = " @" + pr->username;
                    bot.getApi().sendMessage(buffer.front()->chat->id,
                                             "Spam detected" + tag);
                    try {
                        for (const auto &msg : buffer) {
                            if (msg->from->id != pr->id) continue;
#ifdef DEBUG
                            // Can get 'Too many
                            // requests' here
                            if (!msg->text.empty())
                                bot.getApi().sendMessage(
                                    buffer.front()->chat->id,
                                    "Delete '" + msg->text + "'");
#endif
                            bot.getApi().deleteMessage(msg->chat->id,
                                                       msg->messageId);
                        }
                    } catch (std::exception &) {
                        printf("Error deleting msg\n");
                    }
                }
                buffer.clear();
            }).detach();
        }
    });

    auto cleanupFunc = [](int s) {
        if (kCompiler) free(kCompiler);
        if (kCxxCompiler) free(kCxxCompiler);
        printf("Exiting with signal %d\n", s);
        if (tm_ptr) {
            tm_ptr->cancel();
            std::this_thread::sleep_for(std::chrono::seconds(4));
        }
        exit(0);
    };

    std::signal(SIGINT, cleanupFunc);
    std::signal(SIGTERM, cleanupFunc);

    try {
        printf("Bot username: %s\n", bot.getApi().getMe()->username.c_str());
        bot.getApi().deleteWebhook();

        TgLongPoll longPoll(bot);
        while (true) {
            longPoll.start();
        }
    } catch (std::exception &e) {
        // The reason why we exit program instead of continuing is that it
        // will endlessly except if the above code triggered error to
        // Telegram API. Therefore a useless idea to catch and continue.
        // Better to bail out and find problem.
        printf("error: %s\n", e.what());
    }
    cleanupFunc(0);
}
