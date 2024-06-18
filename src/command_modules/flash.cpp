#include <random/RandomNumberGenerator.h>

#include <MessageWrapper.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <chrono>
#include <mutex>
#include <optional>
#include <regex>
#include <thread>

#include "CommandModule.h"
#include "ResourceManager.h"
#include "StringToolsExt.hpp"

constexpr std::string_view kZipExtensionSuffix = ".zip";
constexpr int FLASH_DELAY_MAX_SEC = 5;

static void FlashCommandFn(const Bot& bot, const Message::Ptr& message) {
    static std::vector<std::string> reasons;
    static std::once_flag once;
    static std::regex kFlashTextRegEX(R"(Flashing '\S+.zip'\.\.\.)");
    std::optional<std::string> msg;
    std::stringstream ss;
    Message::Ptr sentmsg;
    MessageWrapper wrapper(bot, message);
    const auto sleep_secs =
        RandomNumberGenerator::generate(FLASH_DELAY_MAX_SEC);
    random_return_type pos;

    std::call_once(once, [] {
        std::string buf;
        buf = ResourceManager::getInstance()->getResource("flash.txt");
        boost::split(reasons, buf, isNewline);
    });

    pos = RandomNumberGenerator::generate(reasons.size());

    if (wrapper.hasReplyToMessage()) {
        wrapper.switchToReplyToMessage();
        if (!wrapper.hasText()) {
            wrapper.sendMessageOnExit("Reply to a text message");
        } else {
            msg = wrapper.getText();
        }
    } else {
        if (!wrapper.hasExtraText()) {
            wrapper.sendMessageOnExit("Send a file name");
        } else {
            msg = wrapper.getExtraText();
        }
    }

    if (!msg) {
        return;
    }

    if (msg->find('\n') != std::string::npos) {
        wrapper.sendMessageOnExit("Invalid input: Zip names shouldn't have newlines");
        return;
    }
    std::replace(msg->begin(), msg->end(), ' ', '_');
    if (!boost::ends_with(msg.value(), kZipExtensionSuffix.data())) {
        msg.value() += kZipExtensionSuffix;
    }
    ss << "Flashing " << std::quoted(msg.value()) << "..." << std::endl;
    sentmsg = bot_sendReplyMessage(bot, message, ss.str());

    std::this_thread::sleep_for(std::chrono::seconds(sleep_secs));
    if (pos != reasons.size()) {
        ss << "Failed successfully!" << std::endl;
        ss << "Reason: " << reasons[pos];
    } else {
        ss << "Success! Chance was 1/" << reasons.size();
    }
    bot_editMessage(bot, sentmsg, ss.str());
}

void loadcmd_flash(CommandModule& module) {
    module.command = "flash";
    module.description = "Flash and get a random result";
    module.flags = CommandModule::Flags::None;
    module.fn = FlashCommandFn;
}