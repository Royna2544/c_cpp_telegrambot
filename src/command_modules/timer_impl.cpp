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
    if (!ctrl->isRunning())
        ctrl->reset();
    ctrl->startTimer(bot, message);
}

static void TimerStopCommandFn(const Bot &bot, const Message::Ptr message) {
    getTMM()->stopTimer(bot, message);
}

struct CommandModule cmd_starttimer("starttimer", "Start timer of the bot",
                                    CommandModule::Flags::Enforced,
                                    TimerStartCommandFn);
struct CommandModule cmd_stoptimer("stoptimer", "Stop timer of the bot",
                                   CommandModule::Flags::Enforced,
                                   TimerStopCommandFn);