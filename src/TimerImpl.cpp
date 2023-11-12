#include <BotReplyMessage.h>
#include <ExtArgs.h>
#include <Timer.h>
#include <utils/libutils.h>

#include <cmath>
#include <optional>

struct Timerpriv {
    MessageId messageid;
    const Bot *bot;
    bool botcanpin, sendendmsg;
    UserId chatid;
};

static std::optional<Timer<Timerpriv>> tm_ptr = std::nullopt;

enum InputState {
    HOUR,
    MINUTE,
    SECOND,
    NONE,
};

using std::chrono_literals::operator""s;

void startTimer(const Bot &bot, const Message::Ptr &message) {
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
                if (!numbercache.empty()) {
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
                    state = InputState::NONE;
                    numbercache.clear();
                }
                break;
            }
            case '0' ... '9': {
                int intver = code - 48;
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
#define TIMER_CONFIG_SEC 5
    if (hms.toSeconds() == 0) {
        bot_sendReplyMessage(bot, message, "I'm not a fool to time 0s");
        return;
    } else if (hms.toSeconds() < TIMER_CONFIG_SEC) {
        bot_sendReplyMessage(bot, message, "Provide longer time value");
        return;
    }
    const int msgid = bot.getApi()
                          .sendMessage(message->chat->id, "Timer starts")
                          ->messageId;
    bool couldpin = true;
    try {
        bot.getApi().pinChatMessage(message->chat->id, msgid);
    } catch (const std::exception &) {
        LOG_W("Cannot pin msg!");
        couldpin = false;
    }
    tm_ptr = std::optional<Timer<Timerpriv>>({hms.h, hms.m, hms.s});
    tm_ptr->setCallback(
        [=](const Timerpriv *priv, struct timehms ms) {
            const auto bot = priv->bot;
            std::stringstream ss;
            if (ms.h != 0) ss << ms.h << "h ";
            if (ms.m != 0) ss << ms.m << "m ";
            if (ms.s != 0) ss << ms.s << "s ";
            if (!ss.str().empty() && ss.str() != message->text && bot)
                bot->getApi().editMessageText(ss.str(), message->chat->id,
                                              priv->messageid);
        },
        TIMER_CONFIG_SEC,
        [=](const Timerpriv *priv) {
            const auto bot = priv->bot;
            if (bot) {
                bot->getApi().editMessageText("Timer ended", message->chat->id,
                                              priv->messageid);
                if (priv->sendendmsg)
                    bot->getApi().sendMessage(message->chat->id, "Timer ended");
                std::this_thread::sleep_for(std::chrono::seconds(1));
                if (priv->botcanpin)
                    bot->getApi().unpinChatMessage(message->chat->id,
                                                   priv->messageid);
            }
            tm_ptr = std::nullopt;
        },
        {msgid, &bot, couldpin, true, message->chat->id});
    tm_ptr->start();
}

void stopTimer(const Bot &bot, const Message::Ptr &message) {
    bool ret = false;
    const char *text = nullptr;
    if (tm_ptr) {
        ret = tm_ptr->cancel([&](Timerpriv *t) -> bool {
            const bool allowed = t->chatid == message->chat->id;
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
}

void forceStopTimer(void) {
    if (tm_ptr) {
        tm_ptr->cancel();
        std::this_thread::sleep_for(4s); // Time for cleanup. e.g. Unpinning
    }
}
