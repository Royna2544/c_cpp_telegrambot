#include <ResourceManager.h>
#include <absl/strings/match.h>
#include <absl/strings/str_replace.h>
#include <absl/strings/str_split.h>
#include <fmt/core.h>

#include <Random.hpp>
#include <api/CommandModule.hpp>
#include <api/Providers.hpp>
#include <api/TgBotApi.hpp>
#include <chrono>
#include <mutex>
#include <optional>
#include <regex>
#include <thread>

#include "StringResLoader.hpp"

constexpr std::string_view kZipExtensionSuffix = ".zip";
constexpr int FLASH_DELAY_MAX_SEC = 5;

DECLARE_COMMAND_HANDLER(flash) {
    static std::vector<std::string> reasons;
    static std::once_flag once;
    std::optional<std::string> msg;
    std::stringstream ss;
    Message::Ptr sentmsg;
    const auto sleep_secs = provider->random->generate(FLASH_DELAY_MAX_SEC);
    Random::ret_type pos = 0;

    std::call_once(once, [provider] {
        std::string buf;
        buf = provider->resource->get("flash.txt");
        reasons = absl::StrSplit(buf, '\n');
    });

    if (message->has<MessageAttrs::ExtraText>()) {
        msg = message->get<MessageAttrs::ExtraText>();
    } else {
        api->sendReplyMessage(message->message(),
                              access(res, Strings::SEND_A_FILE_NAME_TO_FLASH));
        return;
    }
    pos = provider->random->generate(reasons.size());
    if (msg->find('\n') != std::string::npos) {
        api->sendReplyMessage(message->message(),
                              access(res, Strings::INVALID_INPUT_NO_NEWLINE));
        return;
    }
    std::replace(msg->begin(), msg->end(), ' ', '_');
    if (!absl::EndsWith(msg.value(), kZipExtensionSuffix.data())) {
        msg.value() += kZipExtensionSuffix;
    }
    ss << fmt::format("{} '{}'...\n", access(res, Strings::FLASHING_ZIP), msg.value());
    sentmsg = api->sendReplyMessage(message->message(), ss.str());

    std::this_thread::sleep_for(std::chrono::seconds(sleep_secs));
    if (pos != reasons.size()) {
        ss << fmt::format("{}\n{}: {}",
                          access(res, Strings::FAILED_SUCCESSFULLY),
                          access(res, Strings::REASON), reasons[pos]);
    } else {
        ss << fmt::format("{} {:.3}%", access(res, Strings::SUCCESS_CHANCE_WAS),
                          100. / static_cast<int>(reasons.size()));
    }
    api->editMessage(sentmsg, ss.str());
}

DYN_COMMAND_FN(n, module) {
    module.name = "flash";
    module.description = "Flash and get a random result";
    module.flags = CommandModule::Flags::None;
    module.function = COMMAND_HANDLER_NAME(flash);
    return true;
}