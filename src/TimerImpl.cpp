#include <BotReplyMessage.h>
#include <ExtArgs.h>
#include <TimerImpl.h>
#include <absl/log/log.h>
#include <internal/_std_chrono_templates.h>
#include <internal/_tgbot.h>

#include <chrono>
#include <cmath>

#include "SingleThreadCtrl.h"

bool TimerCommandManager::parseTimerArguments(const Bot &bot,
                                              const Message::Ptr &message,
                                              std::chrono::seconds &out) {
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
        bot_sendReplyMessage(bot, message,
                             "Send or reply to a time, in hhmmss format");
        return false;
    }
    if (isactive) {
        bot_sendReplyMessage(bot, message, "Timer is already running");
        return false;
    }
    const char *c_str = msg.c_str();
    std::vector<int> numbercache;
    for (size_t i = 0; i <= msg.size(); i++) {
        int code = static_cast<int>(static_cast<unsigned char>(c_str[i]));
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
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9': {
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
                bot_sendReplyMessage(
                    bot, message,
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
        bot_sendReplyMessage(
            bot, message,
            "Time provided is too long, which is: " + to_string(out));
        return false;
    }
    return true;
}

void TimerCommandManager::TimerThreadFn(const Bot &bot, Message::Ptr message,
                                        std::chrono::seconds timer) {
    isactive = true;
    while (timer > 0s && kRun) {
        std::this_thread::sleep_for(1s);
        if (timer.count() % TIMER_CONFIG_SEC == 0) {
            bot_editMessage(bot, message, to_string(timer));
        }
        timer -= 1s;
    }
    bot_editMessage(bot, message, "Timer ended");
    if (sendendmsg) bot_sendMessage(bot, message->chat->id, "Timer ended");
    if (botcanpin)
        bot.getApi().unpinChatMessage(message->chat->id, message->messageId);
    isactive = false;
}

void TimerCommandManager::startTimer(const Bot &bot, const Message::Ptr &msg) {
    std::chrono::seconds parsedTime(0);

    if (parseTimerArguments(bot, msg, parsedTime)) {
        message = bot_sendMessage(bot, msg->chat->id,
                                  "Timer starting: " + to_string(parsedTime));
        botcanpin = true;
        try {
            bot.getApi().pinChatMessage(message->chat->id, message->messageId);
        } catch (const TgBot::TgException &) {
            LOG(WARNING) <<  "Cannot pin msg!";
            botcanpin = false;
        }
        setPreStopFunction(
            std::bind(&TimerCommandManager::Timerstop, std::placeholders::_1));
        runWith(std::bind(&TimerCommandManager::TimerThreadFn, this,
                          std::cref(bot), message, parsedTime));
    }
}

void TimerCommandManager::stopTimer(const Bot &bot, const Message::Ptr &msg) {
    std::string text;
    if (isactive) {
        const bool allowed = message->chat == msg->chat;
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

void TimerCommandManager::Timerstop(SingleThreadCtrl *thiz) {
    if (static_cast<TimerCommandManager *>(thiz)->isactive) {
        LOG(INFO) << "Canceling timer and cleaning up...";
    }
}
