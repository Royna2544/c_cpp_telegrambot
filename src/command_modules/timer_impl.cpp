#include <SingleThreadCtrl.h>
#include <TimerImpl.h>

#include "CommandModule.h"

static std::shared_ptr<TimerCommandManager> getTMM() {
    return SingleThreadCtrlManager::getInstance()
        .getController<TimerCommandManager>(
            SingleThreadCtrlManager::USAGE_TIMER_THREAD);
}
static void TimerStartCommandFn(const Bot &bot, const Message::Ptr message) {
    auto ctrl = getTMM();
    if (!ctrl->isRunning()) ctrl->reset();
    ctrl->startTimer(bot, message);
}

static void TimerStopCommandFn(const Bot &bot, const Message::Ptr message) {
    getTMM()->stopTimer(bot, message);
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