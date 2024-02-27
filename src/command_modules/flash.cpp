#include <ExtArgs.h>
#include <Logging.h>
#include <ResourceIncBin.h>
#include <StringToolsExt.h>
#include <random/RandomNumberGenerator.h>

#include <mutex>
#include <popen_wdt/popen_wdt.hpp>
#include <regex>

#include "CommandModule.h"

static void FlashCommandFn(const Bot &bot, const Message::Ptr message) {
    static std::vector<std::string> reasons;
    static std::once_flag once;
    static std::regex kFlashTextRegEX(R"(Flashing '\S+.zip'\.\.\.)");
    static const char kZipExtentionSuffix[] = ".zip";
    std::string msg;
    std::stringstream ss;
    Message::Ptr sentmsg;

    std::call_once(once, [] {
        std::string buf, line;
        std::stringstream ss;

        ASSIGN_INCTXT_DATA(FlashTxt, buf);
        splitAndClean(buf, reasons);
    });

    if (message->replyToMessage != nullptr) {
        msg = message->replyToMessage->text;
        if (msg.empty()) {
            bot_sendReplyMessage(bot, message, "Reply to a text");
            return;
        }
    } else {
        if (!hasExtArgs(message)) {
            bot_sendReplyMessage(bot, message, "Send a file name");
            return;
        }
        parseExtArgs(message, msg);
    }
    if (msg.find('\n') != std::string::npos) {
        bot_sendReplyMessage(bot, message, "Invalid input: Zip names shouldn't have newlines");
        return;
    }
    std::replace(msg.begin(), msg.end(), ' ', '_');
    if (!StringTools::endsWith(msg, kZipExtentionSuffix)) {
        msg += kZipExtentionSuffix;
    }
    ss << "Flashing '" << msg << "'..." << std::endl;
    sentmsg = bot_sendReplyMessage(bot, message, ss.str());
    std_sleep_s(genRandomNumber(5));
    if (const size_t pos = genRandomNumber(reasons.size()); pos != reasons.size()) {
        ss << "Failed successfully!" << std::endl;
        ss << "Reason: " << reasons[pos];
    } else {
        ss << "Success! Chance was 1/" << reasons.size();
    }
    bot_editMessage(bot, sentmsg, ss.str());
}

struct CommandModule cmd_flash {
    .enforced = false,
    .name = "flash",
    .fn = FlashCommandFn,
};