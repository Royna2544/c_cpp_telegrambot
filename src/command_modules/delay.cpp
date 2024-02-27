#include <chrono>
#include <ostream>
#include <PrintableTime.h>
#include <BotReplyMessage.h>

#include "CommandModule.h"

static void DelayCommandFn(const Bot &bot, const Message::Ptr message) {
    using std::chrono::duration;
    using std::chrono::high_resolution_clock;
    union time now {
        .val = time(nullptr)
    }, msg{.val = message->date};
    std::ostringstream ss;

    ss << "Request message sent at: " << msg << std::endl;
    ss << "Received at: " << now << " Diff: " << now - msg << 's' << std::endl;
    auto beforeSend = high_resolution_clock::now();
    auto sentMsg = bot_sendReplyMessage(bot, message, ss.str());
    auto afterSend = high_resolution_clock::now();
    ss << "Sending reply message took: " << duration<double, std::milli>(afterSend - beforeSend).count() << "ms" << std::endl;
    bot_editMessage(bot, sentMsg, ss.str());
}

struct CommandModule cmd_delay {
    .enforced = false,
    .name = "delay",
    .fn = DelayCommandFn,
};