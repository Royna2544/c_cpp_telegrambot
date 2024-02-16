#pragma once

#include "NamespaceImport.h"
#include "Types.h"

#include <atomic>
#include <chrono>
#include <thread>

struct TimerCtx {
    static constexpr int TIMER_CONFIG_SEC = 5;

    void startTimer(const Bot &bot, const Message::Ptr& message);
    void stopTimer(const Bot &bot, const Message::Ptr& message);
    void forceStopTimer(void);
 private:
    bool parseTimerArguments(const Bot &bot, const Message::Ptr &message, std::chrono::seconds &out);
    bool botcanpin, sendendmsg, isactive;
    Message::Ptr message;
    std::atomic_bool kStop;
    std::thread threadP;
};
