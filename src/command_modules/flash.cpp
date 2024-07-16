#include <RandomNumberGenerator.hpp>

#include <TgBotWrapper.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <chrono>
#include <mutex>
#include <optional>
#include <regex>
#include <thread>

#include "ResourceManager.h"
#include "StringToolsExt.hpp"

constexpr std::string_view kZipExtensionSuffix = ".zip";
constexpr int FLASH_DELAY_MAX_SEC = 5;

DECLARE_COMMAND_HANDLER(flash, botWrapper, message) {
    static std::vector<std::string> reasons;
    static std::once_flag once;
    static std::regex kFlashTextRegEX(R"(Flashing '\S+.zip'\.\.\.)");
    std::optional<std::string> msg;
    std::stringstream ss;
    Message::Ptr sentmsg;
    MessageWrapper wrapper(message);
    const auto sleep_secs =
        RandomNumberGenerator::generate(FLASH_DELAY_MAX_SEC);
    random_return_type pos = 0;

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
    sentmsg = botWrapper->sendReplyMessage(message, ss.str());

    std::this_thread::sleep_for(std::chrono::seconds(sleep_secs));
    if (pos != reasons.size()) {
        ss << "Failed successfully!" << std::endl;
        ss << "Reason: " << reasons[pos];
    } else {
        ss << "Success! Chance was 1/" << reasons.size();
    }
    botWrapper->editMessage(sentmsg, ss.str());
}

DYN_COMMAND_FN(n, module) {
    module.command = "flash";
    module.description = "Flash and get a random result";
    module.flags = CommandModule::Flags::None;
    module.fn = COMMAND_HANDLER_NAME(flash);
    return true;
}