#include <BotReplyMessage.h>
#include <internal/_std_chrono_templates.h>

#include <chrono>
#include <ctime>
#include <ostream>

#include "CommandModule.h"

static void DelayCommandFn(const Bot& bot, const Message::Ptr message) {
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
    auto sentMsg = bot_sendReplyMessage(bot, message, ss.str());
    auto tp = dp.get();
    ss << "Sending reply message took: " << tp.count() << "ms" << std::endl;
    bot_editMessage(bot, sentMsg, ss.str());
}
   
void loadcmd_delay(CommandModule& module) {
    module.command = "delay";
    module.description = "Ping the bot for network delay";
    module.flags = CommandModule::Flags::None;
    module.fn = DelayCommandFn;
}