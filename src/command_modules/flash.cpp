#include <absl/strings/match.h>
#include <absl/strings/str_split.h>
#include <Random.hpp>
#include <TgBotWrapper.hpp>
#include <chrono>
#include <mutex>
#include <optional>
#include <regex>
#include <thread>

#include "ResourceManager.h"

constexpr std::string_view kZipExtensionSuffix = ".zip";
constexpr int FLASH_DELAY_MAX_SEC = 5;

DECLARE_COMMAND_HANDLER(flash, botWrapper, message) {
    static std::vector<std::string> reasons;
    static std::once_flag once;
    static std::regex kFlashTextRegEX(R"(Flashing '\S+.zip'\.\.\.)");
    std::optional<std::string> msg;
    std::stringstream ss;
    Message::Ptr sentmsg;
    const auto sleep_secs =
        Random::getInstance()->generate(FLASH_DELAY_MAX_SEC);
    Random::ret_type pos = 0;

    std::call_once(once, [] {
        std::string buf;
        buf = ResourceManager::getInstance()->getResource("flash.txt");
        reasons = absl::StrSplit(buf, '\n');
    });

    if (message->replyToMessage_has<MessageExt::Attrs::ExtraText>()) {
        msg = message->replyToMessage->text;
    } else if (message->has<MessageExt::Attrs::ExtraText>()) {
        msg = message->get<MessageExt::Attrs::ExtraText>();
    } else {
        botWrapper->sendReplyMessage(message,
                                     "Reply to a message or send a file name");
        return;
    }
    pos = Random::getInstance()->generate(reasons.size());

    if (msg->find('\n') != std::string::npos) {
        botWrapper->sendReplyMessage(
            message, "Invalid input: Zip names shouldn't have newlines");
        return;
    }
    std::replace(msg->begin(), msg->end(), ' ', '_');
    if (!absl::EndsWith(msg.value(), kZipExtensionSuffix.data())) {
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
    module.function = COMMAND_HANDLER_NAME(flash);
    return true;
}