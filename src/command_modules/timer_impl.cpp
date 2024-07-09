#include <ManagedThreads.hpp>
#include <TimerImpl.hpp>
#include <memory>

#include "BotReplyMessage.h"
#include "CommandModule.h"
#include "TgBotWrapper.hpp"

struct TimerImplThread : TimerThread {
    void onTimerStart() override {
        message = botwrapper->sendMessage(chatid, "Timer started");
    }
    void onTimerPoint(const std::chrono::seconds &timeLeft) override {
        message = botwrapper->editMessage(message,
                                          "Time left: " + to_string(timeLeft));
    }
    void onTimerEnd() override {
        message = botwrapper->editMessage(message, "Timer ended");
    }
    explicit TimerImplThread(ChatId chatid) : chatid(chatid) {}

   private:
    std::shared_ptr<TgBotWrapper> botwrapper = TgBotWrapper::getInstance();
    Message::Ptr message;
    ChatId chatid;
};

static void TimerStartCommandFn(const Bot &bot, const Message::Ptr message) {
    const auto tm = ThreadManager::getInstance();
    auto ctrl = tm->createController<ThreadManager::Usage::TIMER_THREAD,

                                     TimerImplThread>(message->chat->id);
    MessageWrapper wrapper(message);
    bool ok = false;
    std::string result;
    if (wrapper.hasExtraText()) {
        switch (ctrl->parse(wrapper.getExtraText())) {
            case TimerThread::Result::TIME_CANNOT_PARSE:
                result = "Could not parse time: " + wrapper.getExtraText();
                break;
            case TimerThread::Result::TIME_TOO_SHORT:
                result = "Time too short. Please specify a time longer";
                break;
            case TimerThread::Result::TIME_TOO_LONG:
                result = "Time too long. Please specify a time shorter";
                break;
            case TimerThread::Result::SUCCESS:
                result = "Parsed as " + ctrl->describeParsedTime().value();
                ok = true;
                break;
            default:
                CHECK(false) << "Invalid timer result";
                break;
        }
    }
    if (ctrl && ok) {
        ok = false;
        switch (ctrl->start()) {
            case TimerThread::Result::TIMER_STATE_INVALID:
                result = "Timer is already running";
                break;
            case TimerThread::Result::TIMER_NOT_SET:
                result = "Timer is not set";
                break;
            case TimerThread::Result::SUCCESS:
                ok = true;
                ctrl->start();
                break;
            default:
                CHECK(false) << "Invalid timer state";
                break;
        }
    }
    if (!ok) {
        tm->destroyController(ThreadManager::Usage::TIMER_THREAD);
    }
    if (!result.empty()) {
        bot_sendReplyMessage(bot, message, result);
    }
}

static void TimerStopCommandFn(const Bot &bot, const Message::Ptr& message) {
    const auto tm = ThreadManager::getInstance();
    auto ctrl = tm->getController<ThreadManager::Usage::TIMER_THREAD,
                                  TimerImplThread>();
    if (ctrl) {
        ctrl->stop();
        tm->destroyController(ThreadManager::Usage::TIMER_THREAD);
    } else {
        bot_sendReplyMessage(bot, message, "Timer not started");
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