#include <absl/strings/str_split.h>
#include <fmt/format.h>

#include <Random.hpp>
#include <api/CommandModule.hpp>
#include <api/Providers.hpp>
#include <api/TgBotApi.hpp>

#include "api/MessageExt.hpp"

DECLARE_COMMAND_HANDLER(possibility) {
    constexpr int PERCENT_MAX = 100;
    std::string text;
    std::string lastItem;
    std::stringstream outStream;
    std::unordered_map<std::string, Random::ret_type> kItemAndPercentMap;
    std::vector<std::string> vec;
    Random::ret_type total = 0;
    using map_t = std::pair<std::string, Random::ret_type>;

    if (message->get<MessageAttrs::ParsedArgumentsList>().empty()) {
        api->sendReplyMessage(message->message(),
                              res->get(Strings::SEND_POSSIBILITIES));
        return;
    }
    text = message->get<MessageAttrs::ExtraText>();
    // Split string by newline
    vec = message->get<MessageAttrs::ParsedArgumentsList>();
    // Pre-reserve memory
    kItemAndPercentMap.reserve(vec.size());

    auto [b, e] = std::ranges::unique(vec);
    // Create a set of unique items
    vec.erase(b, e);

    // Can't get possitibities for 1 element
    if (vec.size() <= 1) {
        api->sendReplyMessage(message->message(),
                              res->get(Strings::GIVE_MORE_THAN_ONE));
        return;
    }
    // Shuffle the vector.
    provider->random->shuffle(vec);
    // Start the output stream
    outStream << fmt::format("{} {} {}\n", res->get(Strings::TOTAL_ITEMS_PREFIX),
                             vec.size(), res->get(Strings::TOTAL_ITEMS_SUFFIX));
    // Get the last item and remove it from the vector.
    lastItem = vec.back();
    vec.pop_back();
    // Generate all random numbers
    for (const auto &cond : vec) {
        Random::ret_type thisper = 0;
        if (total < PERCENT_MAX) {
            thisper = provider->random->generate(PERCENT_MAX - total);
            if (total + thisper >= PERCENT_MAX) {
                thisper = PERCENT_MAX - total;
            }
        }
        kItemAndPercentMap[cond] = thisper;
        total += thisper;
    }
    // Nonetheless of total being 100 or whatever
    kItemAndPercentMap[lastItem] = PERCENT_MAX - total;
    std::vector<map_t> elem(kItemAndPercentMap.begin(),
                            kItemAndPercentMap.end());
    // Sort by percentages, descending
    std::ranges::sort(elem, [](const map_t &map1, const map_t &map2) {
        if (map1.second != map2.second) {
            return map1.second > map2.second;
        }
        return map1.first < map2.first;
    });
    // Output the results
    for (const map_t &m : elem) {
        outStream << m.first << " : " << m.second << "%" << std::endl;
    }
    api->sendReplyMessage(message->message(), outStream.str());
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
