#include <internal/_std_chrono_templates.h>

#include <TgBotWrapper.hpp>
#include <chrono>
#include <ctime>
#include <ostream>

#include "DurationPoint.hpp"

DECLARE_COMMAND_HANDLER(delay, wrapper, message) {
    using std::chrono::duration;
    using std::chrono::high_resolution_clock;
    using std::chrono::system_clock;
    std::ostringstream ss;
    auto msg = system_clock::from_time_t(message->date);
    auto now = system_clock::now();

    ss << "Request message sent at: " << Time_t{message->date} << std::endl;
    ss << "Received at: " << fromTP(now) << std::endl;
    ss << "Diff: " << to_secs(now - msg).count() << 's' << std::endl;
    auto dp = DurationPoint();
    auto sentMsg = wrapper->sendReplyMessage(message, ss.str());
    auto tp = dp.get();
    ss << "Sending reply message took: " << tp.count() << "ms" << std::endl;
    // Update the sent message with the delay information
    wrapper->editMessage(sentMsg, ss.str());
}

DYN_COMMAND_FN(/*name*/, module) {
    module.command = "delay";
    module.description = "Ping the bot for network delay";
    module.flags = CommandModule::Flags::None;
    module.function = COMMAND_HANDLER_NAME(delay);
    return true;
}