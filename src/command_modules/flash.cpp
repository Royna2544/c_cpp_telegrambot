#include <absl/log/log.h>
#include <absl/strings/match.h>
#include <absl/strings/str_replace.h>
#include <absl/strings/str_split.h>
#include <fmt/core.h>

#include <Random.hpp>
#include <api/CommandModule.hpp>
#include <api/Providers.hpp>
#include <api/StringResLoader.hpp>
#include <api/TgBotApi.hpp>
#include <chrono>
#include <mutex>
#include <optional>
#include <regex>

constexpr std::string_view kZipExtensionSuffix = ".zip";
constexpr int FLASH_DELAY_MAX_SEC = 5;
constexpr float SUCCESS_CHANCE = 0.1F;

DECLARE_COMMAND_HANDLER(flash) {
    static std::vector<std::string> reasons;
    static std::once_flag once;
    std::optional<std::string> msg;
    const auto sleep_secs = provider->random->generate(FLASH_DELAY_MAX_SEC);
    Random::ret_type pos = 0;

    std::call_once(once, [provider] {
        std::string buf;
        buf = provider->resource->get("flash.txt");
        reasons = absl::StrSplit(buf, '\n');
    });

    if (reasons.empty()) {
        LOG(ERROR) << "Flash command: reasons list is empty, cannot proceed";
        return;
    }

    if (message->has<MessageAttrs::ExtraText>()) {
        msg = message->get<MessageAttrs::ExtraText>();
    } else {
        api->sendReplyMessage(message->message(),
                              res->get(Strings::SEND_A_FILE_NAME_TO_FLASH));
        return;
    }
    pos = provider->random->generate(reasons.size() - 1);
    if (msg->find('\n') != std::string::npos) {
        api->sendReplyMessage(message->message(),
                              res->get(Strings::INVALID_INPUT_NO_NEWLINE));
        return;
    }
    std::replace(msg->begin(), msg->end(), ' ', '_');
    if (!absl::EndsWith(msg.value(), kZipExtensionSuffix.data())) {
        msg.value() += kZipExtensionSuffix;
    }
    const std::string initial =
        fmt::format("{} '{}'...\n", res->get(Strings::FLASHING_ZIP), *msg);
    std::string final = initial;
    constexpr Random::ret_type kChanceScale = 1000;
    const bool succeeded =
        provider->random->generate(0, kChanceScale - 1) <
        static_cast<Random::ret_type>(SUCCESS_CHANCE * kChanceScale);
    if (!succeeded) {
        final +=
            fmt::format("{}\n{}: {}", res->get(Strings::FAILED_SUCCESSFULLY),
                        res->get(Strings::REASON), reasons[pos]);
    } else {
        final += fmt::format("{} {:.3}%", res->get(Strings::SUCCESS_CHANCE_WAS),
                             SUCCESS_CHANCE * 100);
    }
    const auto source = message->message();
    if (!api->submitCommandWork(
            "flash", TgBotApi::WorkClass::Outbound,
            [api, source, initial, final = std::move(final),
             sleep_secs](std::stop_token stop) mutable {
                if (stop.stop_requested())
                    return;
                const auto sent = api->sendReplyMessage(source, initial);
                if (!sent || stop.stop_requested())
                    return;
                if (!api->submitCommandWork(
                        "flash", TgBotApi::WorkClass::Outbound,
                        [api, sent,
                         final = std::move(final)](std::stop_token editStop) {
                            if (!editStop.stop_requested())
                                api->editMessage(sent, final);
                        },
                        {.delay = std::chrono::seconds(sleep_secs),
                         .deadline = std::chrono::seconds(10)})) {
                    LOG(WARNING) << "Flash result queue is full";
                }
            })) {
        LOG(WARNING) << "Flash outbound queue is full";
    }
}

extern "C" DYN_COMMAND_EXPORT const struct DynModule DYN_COMMAND_SYM = {
    .flags = DynModule::Flags::None,
    .name = "flash",
    .description = "Flash and get a random result",
    .function = COMMAND_HANDLER_NAME(flash),
    .valid_args = {
        .enabled = true,
        .counts = DynModule::craftArgCountMask<1>(),
        .split_type = DynModule::ValidArgs::Split::None,
        .usage = "/flash [filename-to-flash]",
    }};
