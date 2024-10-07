#include <fmt/chrono.h>

#include <ManagedThreads.hpp>
#include <memory>

#include "TgBotWrapper.hpp"
#include "support/timer/TimerImpl.hpp"

struct TimerImplThread : TimerThread {
    void onTimerStart() override {
        message = botwrapper->sendMessage(chatid, "Timer started");
    }
    void onTimerPoint(const std::chrono::seconds &timeLeft) override {
        message = botwrapper->editMessage(
            message, fmt::format("Time left: {}", timeLeft));
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

DECLARE_COMMAND_HANDLER(starttimer, bot, message) {
    const auto tm = ThreadManager::getInstance();
    auto ctrl = tm->createController<ThreadManager::Usage::TIMER_THREAD,

                                     TimerImplThread>(message->chat->id);
    bool ok = false;
    std::string result;
    if (message->has<MessageExt::Attrs::ExtraText>()) {
        switch (ctrl->parse(message->text)) {
            case TimerThread::Result::TIME_CANNOT_PARSE:
                result = "Could not parse time: " + message->text;
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
        bot->sendReplyMessage(message, result);
    }
}

DECLARE_COMMAND_HANDLER(stoptimer, bot, message) {
    const auto tm = ThreadManager::getInstance();
    auto ctrl = tm->getController<ThreadManager::Usage::TIMER_THREAD,
                                  TimerImplThread>();
    if (ctrl) {
        ctrl->stop();
        tm->destroyController(ThreadManager::Usage::TIMER_THREAD);
    } else {
        bot->sendReplyMessage(message, "Timer not started");
    }
}

DYN_COMMAND_FN(name, module) {
    if (name == "starttimer") {
        module.command = "starttimer";
        module.description = "Start timer of the bot";
        module.function = COMMAND_HANDLER_NAME(starttimer);
    } else if (name == "stoptimer") {
        module.command = "stoptimer";
        module.description = "Stop timer of the bot";
        module.function = COMMAND_HANDLER_NAME(stoptimer);
    } else {
        return false;
    }
    module.flags = CommandModule::Flags::Enforced;
    return true;
}
