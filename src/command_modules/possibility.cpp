#include <absl/strings/str_split.h>
#include <fmt/format.h>

#include <Random.hpp>
#include <algorithm>
#include <api/CommandModule.hpp>
#include <api/Providers.hpp>
#include <api/TgBotApi.hpp>
#include <cctype>
#include <string_view>
#include <unordered_set>

#include "api/MessageExt.hpp"

namespace {

constexpr Random::ret_type kPercentMax = 100;
constexpr std::size_t kMaxChoices = 100;
constexpr std::size_t kMaxChoiceBytes = 64;
constexpr std::size_t kMaxTelegramTextBytes = 4096;

bool isValidChoice(const std::string_view choice) {
    if (choice.empty() || choice.size() > kMaxChoiceBytes) {
        return false;
    }
    return std::ranges::none_of(
        choice, [](const unsigned char ch) { return std::iscntrl(ch) != 0; });
}

std::string validationError(const StringResLoader::PerLocaleMap* res,
                            const std::string_view detail) {
    return fmt::format("{}: {}", res->get(Strings::INVALID_ARGS_PASSED),
                       detail);
}

}  // namespace

DECLARE_COMMAND_HANDLER(possibility) {
    std::stringstream outStream;
    using WeightedChoice = std::pair<std::string, Random::ret_type>;

    const auto& parsed = message->get<MessageAttrs::ParsedArgumentsList>();
    if (parsed.empty()) {
        api->sendReplyMessage(message->message(),
                              res->get(Strings::SEND_POSSIBILITIES));
        return;
    }

    // Preserve the first occurrence of each choice. ranges::unique only
    // removed adjacent duplicates, allowing repeated non-adjacent choices to
    // overwrite one another after percentages had already been allocated.
    std::vector<std::string> choices;
    choices.reserve(std::min(parsed.size(), kMaxChoices));
    std::unordered_set<std::string> seen;
    seen.reserve(std::min(parsed.size(), kMaxChoices));
    for (const auto& choice : parsed) {
        if (!isValidChoice(choice)) {
            api->sendReplyMessage(
                message->message(),
                validationError(
                    res, fmt::format("each choice must contain 1-{} bytes of "
                                     "printable text",
                                     kMaxChoiceBytes)));
            return;
        }
        if (!seen.emplace(choice).second) {
            continue;
        }
        if (choices.size() == kMaxChoices) {
            api->sendReplyMessage(
                message->message(),
                validationError(
                    res, fmt::format("at most {} unique choices are allowed",
                                     kMaxChoices)));
            return;
        }
        choices.emplace_back(choice);
    }

    if (choices.size() <= 1) {
        api->sendReplyMessage(message->message(),
                              res->get(Strings::GIVE_MORE_THAN_ONE));
        return;
    }

    provider->random->shuffle(choices);
    outStream << fmt::format(
        "{} {} {}\n", res->get(Strings::TOTAL_ITEMS_PREFIX), choices.size(),
        res->get(Strings::TOTAL_ITEMS_SUFFIX));

    // Keep the original skewed allocation: each item receives a random part
    // of the remaining percentage, and the final item receives the remainder.
    // Shuffling first keeps that positional skew fair across the choices.
    std::vector<WeightedChoice> weighted;
    weighted.reserve(choices.size());
    Random::ret_type remaining = kPercentMax;
    for (std::size_t i = 0; i < choices.size(); ++i) {
        Random::ret_type percent = remaining;
        if (i + 1 != choices.size() && remaining != 0) {
            percent =
                std::min(provider->random->generate(0, remaining), remaining);
        }
        weighted.emplace_back(std::move(choices[i]), percent);
        remaining -= percent;
    }

    std::ranges::sort(
        weighted, [](const WeightedChoice& map1, const WeightedChoice& map2) {
            if (map1.second != map2.second) {
                return map1.second > map2.second;
            }
            return map1.first < map2.first;
        });

    Random::ret_type displayedTotal = 0;
    for (const auto& m : weighted) {
        outStream << m.first << " : " << m.second << "%" << std::endl;
        displayedTotal += m.second;
    }
    if (displayedTotal != kPercentMax) {
        api->sendReplyMessage(
            message->message(),
            validationError(res, "failed to allocate exactly 100 percent"));
        return;
    }
    outStream << res->get(Strings::TOTAL_ITEMS_PREFIX) << ": " << displayedTotal
              << "%";
    auto output = outStream.str();
    if (output.size() > kMaxTelegramTextBytes) {
        api->sendReplyMessage(
            message->message(),
            validationError(res, fmt::format("combined output exceeds {} bytes",
                                             kMaxTelegramTextBytes)));
        return;
    }
    api->sendReplyMessage(message->message(), output);
}

extern "C" DYN_COMMAND_EXPORT const struct DynModule DYN_COMMAND_SYM = {
    .flags = DynModule::Flags::None,
    .name = "possibility",
    .description = "Get possibilities",
    .function = COMMAND_HANDLER_NAME(possibility),
    .valid_args = {
        .enabled = true,
        .split_type = DynModule::ValidArgs::Split::ByNewline,
        .usage = "/possibility conditions-by-newline",
    }};
