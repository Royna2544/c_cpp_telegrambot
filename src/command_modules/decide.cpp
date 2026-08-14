#include <absl/log/log.h>
#include <fmt/core.h>

#include <Random.hpp>
#include <api/CommandModule.hpp>
#include <api/Providers.hpp>
#include <api/StringResLoader.hpp>
#include <api/TgBotApi.hpp>
#include <chrono>
#include <exception>
#include <memory>
#include <optional>
#include <sstream>

#include "tgbot/types/ReactionTypeEmoji.h"

DECLARE_COMMAND_HANDLER(decide) {
    constexpr int kTrialCount = 10;

    std::string obj = message->get<MessageAttrs::ExtraText>();
    std::stringstream finalText;
    std::stringstream progressText;
    int yesno = 0;

    const std::string heading = fmt::format(
        fmt::runtime(res->get(Strings::DECIDE_DECIDING_OBJECT)), obj);
    finalText << heading << std::endl << std::endl;
    progressText << heading << std::endl << std::endl;
    for (int trial = 1; trial <= kTrialCount; ++trial) {
        std::string line = fmt::format(
            fmt::runtime(res->get(Strings::DECIDE_TRY_PREFIX)), trial);

        // Ask the RNG for an actual coin flip. The previous 0..10 draw followed
        // by modulo had six even outcomes and five odd ones, biasing "No".
        if (provider->random->generate(0, 1) == 1) {
            line += res->get(Strings::YES);
            ++yesno;
        } else {
            line += res->get(Strings::NO);
            --yesno;
        }
        finalText << line << std::endl;
        if (trial <= kTrialCount / 2)
            progressText << line << std::endl;
    }
    progressText << "…";

    finalText << std::endl;
    std::optional<std::string> reaction;
    if (yesno > 0) {
        finalText << res->get(Strings::SO_YES);
        reaction = "👍";
    } else if (yesno == 0) {
        finalText << res->get(Strings::SO_IDK);
    } else {
        finalText << res->get(Strings::SO_NO);
        reaction = "👎";
    }

    const auto sourceMessage = message->message();
    const auto job = api->submitCommandWork(
        "decide", TgBotApi::WorkClass::Outbound,
        [api, sourceMessage, heading, progress = progressText.str(),
         final = finalText.str(),
         reaction = std::move(reaction)](std::stop_token stop) {
            if (stop.stop_requested())
                return;
            const auto started = std::chrono::steady_clock::now();
            const auto sent = api->sendReplyMessage(sourceMessage, heading);
            if (!sent || stop.stop_requested())
                return;
            const auto remainingDelay = [started](const auto targetElapsed) {
                const auto due = started + targetElapsed;
                const auto now = std::chrono::steady_clock::now();
                return now < due ? std::chrono::duration_cast<
                                       std::chrono::milliseconds>(due - now)
                                 : std::chrono::milliseconds::zero();
            };

            if (!api->submitCommandWork(
                    "decide", TgBotApi::WorkClass::Outbound,
                    [api, sent, progress = std::move(progress)](
                        std::stop_token progressStop) {
                        if (!progressStop.stop_requested())
                            api->editMessage(sent, progress);
                    },
                    {.delay = remainingDelay(std::chrono::milliseconds(350)),
                     .deadline = std::chrono::seconds(2)})) {
                LOG(WARNING) << "Decision progress queue is full";
            }

            if (!api->submitCommandWork(
                    "decide", TgBotApi::WorkClass::Outbound,
                    [api, sourceMessage, sent, final = std::move(final),
                     reaction =
                         std::move(reaction)](std::stop_token finalStop) {
                        if (finalStop.stop_requested())
                            return;
                        // Persist the result before a best-effort reaction; a
                        // reaction failure must never hide the decision.
                        api->editMessage(sent, final);
                        if (!reaction || finalStop.stop_requested())
                            return;
                        try {
                            auto emoji =
                                std::make_shared<TgBot::ReactionTypeEmoji>();
                            emoji->emoji = *reaction;
                            (void)api->setMessageReaction(sourceMessage,
                                                          {emoji}, true);
                        } catch (const std::exception& error) {
                            LOG(WARNING)
                                << "Decision reaction failed after final edit: "
                                << error.what();
                        }
                    },
                    {.delay = remainingDelay(std::chrono::milliseconds(850)),
                     .deadline = std::chrono::seconds(2)})) {
                LOG(WARNING) << "Decision final queue is full";
            }
        },
        {.deadline = std::chrono::seconds(2)});
    if (!job) {
        LOG(WARNING) << "Decision outbound queue is full";
    }
}

extern "C" DYN_COMMAND_EXPORT const struct DynModule DYN_COMMAND_SYM = {
    .flags = DynModule::Flags::None,
    .name = "decide",
    .description = "Decide a statement",
    .function = COMMAND_HANDLER_NAME(decide),
    .valid_args =
        {
            .enabled = true,
            .counts = DynModule::craftArgCountMask<1>(),
            .split_type = DynModule::ValidArgs::Split::None,
            .usage = "/decide <statement>",
        },
};
