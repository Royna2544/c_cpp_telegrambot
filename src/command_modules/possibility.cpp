#include <absl/strings/str_split.h>

#include <Random.hpp>
#include <api/CommandModule.hpp>
#include <api/TgBotApi.hpp>
#include <set>

#include "StringToolsExt.hpp"

DECLARE_COMMAND_HANDLER(possibility, botWrapper, message) {
    constexpr int PERCENT_MAX = 100;
    std::string text;
    std::string lastItem;
    std::stringstream outStream;
    std::unordered_map<std::string, Random::ret_type> kItemAndPercentMap;
    std::vector<std::string> vec;
    std::set<std::string> set;
    Random::ret_type total = 0;
    using map_t = std::pair<std::string, Random::ret_type>;

    if (!message->has<MessageAttrs::ExtraText>()) {
        botWrapper->sendReplyMessage(
            message->message(), "Send avaliable conditions sperated by newline");
        return;
    }
    text = message->get<MessageAttrs::ExtraText>();
    // Split string by newline
    set = absl::StrSplit(text, '\n', absl::SkipWhitespace());
    // Pre-reserve memory
    kItemAndPercentMap.reserve(set.size());
    // Can't get possitibities for 1 element
    if (set.size() == 1) {
        botWrapper->sendReplyMessage(message->message(), "Give more than 1 choice");
        return;
    }
    // Put it in vector again and shuffle it.
    vec = {set.begin(), set.end()};
    // Shuffle the vector.
    Random::getInstance()->shuffleArray(vec);
    // Start the output stream
    outStream << "Total " << vec.size() << " items" << std::endl;
    // Get the last item and remove it from the vector.
    lastItem = vec.back();
    vec.pop_back();
    // Generate all random numbers
    for (const auto &cond : vec) {
        Random::ret_type thisper = 0;
        if (total < PERCENT_MAX) {
            thisper = Random::getInstance()->generate(PERCENT_MAX - total);
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
    botWrapper->sendReplyMessage(message->message(), outStream.str());
}

DYN_COMMAND_FN(n, module) {
    module.name = "possibility";
    module.description = "Get possibilities";
    module.flags = CommandModule::Flags::None;
    module.function = COMMAND_HANDLER_NAME(possibility);
    return true;
}