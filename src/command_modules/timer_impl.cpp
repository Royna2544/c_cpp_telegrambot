#include <TimerImpl.h>

#include <ManagedThreads.hpp>

#include "CommandModule.h"

static void TimerStartCommandFn(const Bot &bot, const Message::Ptr message) {
    const auto tm = ThreadManager::getInstance();
    auto ctrl = tm->createController<ThreadManager::Usage::TIMER_THREAD,
                                     TimerCommandManager>();
    if (ctrl) {
        ctrl->startTimer(bot, message);
    } else {
        ctrl = tm->getController<ThreadManager::Usage::TIMER_THREAD, TimerCommandManager>();
        CHECK(ctrl) << "Timer should be gettable";
        ctrl->startTimer(bot, message);
    }
}

static void TimerStopCommandFn(const Bot &bot, const Message::Ptr message) {
    const auto tm = ThreadManager::getInstance();
    auto ctrl = tm->getController<ThreadManager::Usage::TIMER_THREAD,
                                     TimerCommandManager>();
    if (ctrl) {
        ctrl->stopTimer(bot, message);
    } else {
        ctrl = tm->createController<ThreadManager::Usage::TIMER_THREAD, TimerCommandManager>();
        CHECK(ctrl) << "Timer should be gettable";
        ctrl->stopTimer(bot, message);
        tm->destroyController(ThreadManager::Usage::TIMER_THREAD);
    }
}

void loadcmd_starttimer(CommandModule &module) {
    module.command = "starttimer";
    module.description = "Start timer of the bot";
    module.flags = CommandModule::Flags::Enforced;
    module.fn = TimerStartCommandFn;
}

void loadcmd_stoptimer(CommandModule &module) {
    module.command = "stoptimer";
    module.description = "Stop timer of the bot";
    module.flags = CommandModule::Flags::Enforced;
    module.fn = TimerStopCommandFn;
}