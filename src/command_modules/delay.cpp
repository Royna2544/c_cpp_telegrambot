#include <BotReplyMessage.h>
#include <internal/_std_chrono_templates.h>

#include <chrono>
#include <ctime>
#include <ostream>

#include "CommandModule.h"

union Time_t {
    time_t val;
};

std::ostream& operator<<(std::ostream& self, const Time_t tp) {
    self << std::put_time(std::gmtime(&tp.val), "%Y-%m-%dT%H:%M:%SZ (GMT)");
    return self;
}

template <class T>
Time_t fromTP(std::chrono::time_point<T> it) {
    return {std::chrono::system_clock::to_time_t(it)};
}

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
    auto beforeSend = high_resolution_clock::now();
    auto sentMsg = bot_sendReplyMessage(bot, message, ss.str());
    auto afterSend = high_resolution_clock::now();
    ss << "Sending reply message took: "
       << duration<double, std::milli>(afterSend - beforeSend).count() << "ms"
       << std::endl;
    bot_editMessage(bot, sentMsg, ss.str());
}

struct CommandModule cmd_delay {
    .enforced = false, .name = "delay", .fn = DelayCommandFn,
};