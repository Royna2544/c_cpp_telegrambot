#pragma once

#include <tgbot/Bot.h>
#include <tgbot/types/Message.h>

#include "SingleThreadCtrl.h"

#include <chrono>

using TgBot::Bot;
using TgBot::Message;

struct TimerCommandManager : SingleThreadCtrl {
    static constexpr int TIMER_CONFIG_SEC = 5;
    TimerCommandManager() : SingleThreadCtrl() {}
    ~TimerCommandManager() override = default;

    void startTimer(const Bot &bot, const Message::Ptr& message);
    void stopTimer(const Bot &bot, const Message::Ptr& message);
    static void Timerstop(SingleThreadCtrl *);
 private:
    void TimerThreadFn(const Bot &bot, Message::Ptr message, std::chrono::seconds s);
    bool parseTimerArguments(const Bot &bot, const Message::Ptr &message, std::chrono::seconds &out);
    bool botcanpin = true, sendendmsg = true, isactive = false;
    Message::Ptr message;
};
