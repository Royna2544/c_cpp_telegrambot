#include <ExtArgs.h>
#include <StringToolsExt.h>
#include <random/RandomNumberGenerator.h>

#include <chrono>
#include <mutex>
#include <regex>
#include <thread>

#include "CommandModule.h"
#include "ResourceManager.h"

constexpr std::string_view kZipExtensionSuffix = ".zip";
constexpr int FLASH_DELAY_MAX_SEC = 5;

static void FlashCommandFn(const Bot &bot, const Message::Ptr message) {
    static std::vector<std::string> reasons;
    static std::once_flag once;
    static std::regex kFlashTextRegEX(R"(Flashing '\S+.zip'\.\.\.)");
    std::string msg;
    std::stringstream ss;
    Message::Ptr sentmsg;

    std::call_once(once, [] {
        std::string buf, line;
        std::stringstream ss;

        buf = ResourceManager::getInstance().getResource("flash.txt");
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
        bot_sendReplyMessage(
            bot, message, "Invalid input: Zip names shouldn't have newlines");
        return;
    }
    std::replace(msg.begin(), msg.end(), ' ', '_');
    if (!StringTools::endsWith(msg, std::string(kZipExtensionSuffix))) {
        msg += kZipExtensionSuffix;
    }
    ss << "Flashing '" << msg << "'..." << std::endl;
    sentmsg = bot_sendReplyMessage(bot, message, ss.str());
    std::this_thread::sleep_for(std::chrono::seconds(genRandomNumber(FLASH_DELAY_MAX_SEC)));
    if (const random_return_type pos = genRandomNumber(reasons.size());
        pos != reasons.size()) {
        ss << "Failed successfully!" << std::endl;
        ss << "Reason: " << reasons[pos];
    } else {
        ss << "Success! Chance was 1/" << reasons.size();
    }
    bot_editMessage(bot, sentmsg, ss.str());
}

struct CommandModule cmd_flash("flash", "Flash and get a random result",
                               CommandModule::Flags::None, FlashCommandFn);