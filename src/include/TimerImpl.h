#pragma once

#include "NamespaceImport.h"
#include "Types.h"

#include <atomic>
#include <memory>
#include <thread>

struct TimerCtx {
    bool botcanpin, sendendmsg, isactive;
    Message::Ptr message;
    std::atomic_bool kStop;
    std::thread threadP;
};

void startTimer(const Bot &bot, const Message::Ptr message, const std::shared_ptr<TimerCtx> ctx);
void stopTimer(const Bot &bot, const Message::Ptr message, const std::shared_ptr<TimerCtx> ctx);
void forceStopTimer(const std::shared_ptr<TimerCtx> ctx);
