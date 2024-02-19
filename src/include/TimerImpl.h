#pragma once

#include "NamespaceImport.h"
#include "Types.h"
#include "SingleThreadCtrl.h"

#include <chrono>

struct TimerCtx : SingleThreadCtrl {
    static constexpr int TIMER_CONFIG_SEC = 5;
    TimerCtx(thread_function other) : SingleThreadCtrl(other) {}
    TimerCtx() : SingleThreadCtrl() {}

    void startTimer(const Bot &bot, const Message::Ptr& message);
    void stopTimer(const Bot &bot, const Message::Ptr& message);
    void forceStopTimer(void);
 private:
    void TimerThreadFn(const Bot &bot, Message::Ptr message, std::chrono::seconds s);
    bool parseTimerArguments(const Bot &bot, const Message::Ptr &message, std::chrono::seconds &out);
    bool botcanpin, sendendmsg, isactive;
    Message::Ptr message;
};
