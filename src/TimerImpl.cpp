#include <BotReplyMessage.h>
#include <ExtArgs.h>
#include <Logging.h>
#include <NamespaceImport.h>
#include <TimerImpl.h>

#include <chrono>
#include <cmath>
#include <sstream>
#include "SingleThreadCtrl.h"

template <class Dur>
std::chrono::hours to_hours(Dur &&it) {
    return std::chrono::duration_cast<std::chrono::hours>(it);
}

template <class Dur>
std::chrono::minutes to_mins(Dur &&it) {
    return std::chrono::duration_cast<std::chrono::minutes>(it);
}

template <class Dur>
std::chrono::seconds to_secs(Dur &&it) {
    return std::chrono::duration_cast<std::chrono::seconds>(it);
}

template <class Dur>
std::string to_string(const Dur out) {
    const auto hms = std::chrono::hh_mm_ss(out);
    std::stringstream ss;

    ss << hms.hours().count() << "h " << hms.minutes().count() << "m " << hms.seconds().count() << "s";
    return ss.str();
}

bool TimerCommandManager::parseTimerArguments(const Bot &bot, const Message::Ptr &message, std::chrono::seconds &out) {
    bool found = false;
    std::string msg;
    enum {
        HOUR,
        MINUTE,
        SECOND,
        NONE,
    } state = NONE;

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
        return false;
    }
    if (isactive) {
        bot_sendReplyMessage(bot, message, "Timer is already running");
        return false;
    }
    const char *c_str = msg.c_str();
    std::vector<int> numbercache;
    for (size_t i = 0; i <= msg.size(); i++) {
        int code = static_cast<int>(c_str[i]);
        if (i == msg.size()) code = ' ';
        switch (code) {
            case ' ': {
                if (!numbercache.empty()) {
                    int result = 0, count = 1;
                    for (const auto i : numbercache) {
                        result += pow(10, numbercache.size() - count) * i;
                        count++;
                    }
                    switch (state) {
                        case HOUR:
                            out += to_secs(std::chrono::hours(result));
                            break;
                        case MINUTE:
                            out += to_secs(std::chrono::minutes(result));
                            break;
                        case SECOND:
                            out += to_secs(std::chrono::seconds(result));
                            break;
                        default:
                            break;
                    }
                    state = NONE;
                    numbercache.clear();
                }
                break;
            }
            case '0' ... '9': {
                int intver = code - '0';
                numbercache.push_back(intver);
                break;
            }
            case 'H':
            case 'h': {
                state = HOUR;
                break;
            }
            case 'M':
            case 'm': {
                state = MINUTE;
                break;
            }
            case 'S':
            case 's': {
                state = SECOND;
                break;
            }
            default: {
                bot_sendReplyMessage(bot, message,
                                     "Invalid value provided.\nShould contain only h, m, s, "
                                     "numbers, spaces. (ex. 1h 20m 7s)");

                return false;
            }
        }
    }
    if (out.count() == 0) {
        bot_sendReplyMessage(bot, message, "I'm not a fool to time 0s");
        return false;
    } else if (out.count() < TIMER_CONFIG_SEC) {
        bot_sendReplyMessage(bot, message, "Provide longer time value");
        return false;
    } else if (to_hours(out).count() > 2) {
        bot_sendReplyMessage(bot, message,
                             "Time provided is too long, which is: " + to_string(out));
        return false;
    }
    return true;
}

void TimerCommandManager::TimerThreadFn(const Bot &bot, Message::Ptr message, std::chrono::seconds timer) {
    isactive = true;
    while (timer > 0s && kRun) {
        std_sleep_s(1);
        if (timer.count() % TIMER_CONFIG_SEC == 0) {
            bot_editMessage(bot, message, to_string(timer));
        }
        timer -= 1s;
    }
    bot_editMessage(bot, message, "Timer ended");
    if (sendendmsg)
        bot_sendMessage(bot, message->chat->id, "Timer ended");
    if (botcanpin)
        bot.getApi().unpinChatMessage(message->chat->id, message->messageId);
    isactive = false;
}

void TimerCommandManager::startTimer(const Bot &bot, const Message::Ptr& msg) {
    std::chrono::seconds parsedTime(0);

    if (parseTimerArguments(bot, msg, parsedTime)) {
        message = bot_sendMessage(bot, msg->chat->id, "Timer starting: " + to_string(parsedTime));
        botcanpin = true;
        try {
            bot.getApi().pinChatMessage(message->chat->id, message->messageId);
        } catch (const TgBot::TgException &) {
            LOG_W("Cannot pin msg!");
            botcanpin = false;
        }
        setThreadFunction(std::bind(&TimerCommandManager::TimerThreadFn, this, std::cref(bot), message, parsedTime));
    }
}

void TimerCommandManager::stopTimer(const Bot &bot, const Message::Ptr& message) {
    std::string text;
    if (isactive) {
        const bool allowed = message->chat->id == message->chat->id;
        sendendmsg = !allowed;
        if (allowed) {
            stop();
            text = "Stopped successfully";
        } else {
            text = "Timer is running on other group.";
        }
    } else {
        text = "Timer is not running";
    }
    bot_sendReplyMessage(bot, message, text);
}

void TimerCommandManager::stop() {
    if (isactive) {
        LOG_I("Canceling timer and cleaning up...");
        SingleThreadCtrl::stop();
    }
}
