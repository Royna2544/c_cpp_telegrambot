#include <fmt/chrono.h>
#include <fmt/core.h>
#include <tgbot/TgException.h>
#include <trivial_helpers/_std_chrono_templates.h>

#include <DurationPoint.hpp>
#include <api/CommandModule.hpp>
#include <api/MessageExt.hpp>
#include <api/TgBotApi.hpp>
#include <chrono>
#include <ctime>
#include <sstream>
#include <string>

DECLARE_COMMAND_HANDLER(delay, wrapper, message) {
    using std::chrono::duration;
    using std::chrono::high_resolution_clock;
    using std::chrono::system_clock;
    std::stringstream ss;
    auto msgTP = message->get<MessageAttrs::Date>();
    auto nowTP = system_clock::now();

    ss << fmt::format(
        "Request message sent at: {}\n"
        "Received at: {}\n"
        "Difference: {}\n",
        msgTP, nowTP, to_secs(nowTP - msgTP));
    auto dp = DurationPoint();
    auto sentMsg = wrapper->sendReplyMessage(message->message(), ss.str());
    ss << fmt::format("Sending reply message took: {}", dp.get());
    // Update the sent message with the delay information
    wrapper->editMessage(sentMsg, ss.str());
}

DYN_COMMAND_FN(/*name*/, module) {
    module.name = "delay";
    module.description = "Ping the bot for network delay";
    module.flags = CommandModule::Flags::None;
    module.function = COMMAND_HANDLER_NAME(delay);
    return true;
}