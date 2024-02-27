#include <SingleThreadCtrl.h>
#include <TimerImpl.h>

#include "CommandModule.h"

static void TimerStartCommandFn(const Bot &bot, const Message::Ptr message) {
    gSThreadManager
        .getController<TimerCommandManager>(SingleThreadCtrlManager::USAGE_TIMER_THREAD)
        ->startTimer(bot, message);
}

static void TimerStopCommandFn(const Bot &bot, const Message::Ptr message) {
    gSThreadManager
        .getController<TimerCommandManager>(SingleThreadCtrlManager::USAGE_TIMER_THREAD)
        ->stopTimer(bot, message);
    gSThreadManager.destroyController(SingleThreadCtrlManager::USAGE_TIMER_THREAD);
}

struct CommandModule cmd_starttimer {
    .enforced = true,
    .name = "starttimer",
    .fn = TimerStartCommandFn,
};

struct CommandModule cmd_stoptimer {
    .enforced = true,
    .name = "stoptimer",
    .fn = TimerStopCommandFn,
};