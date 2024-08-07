#include <Random.hpp>

#include <TgBotWrapper.hpp>
#include <boost/algorithm/string/split.hpp>
#include <set>

#include "StringToolsExt.hpp"

DECLARE_COMMAND_HANDLER(possibility, botWrapper, message){
    constexpr int PERCENT_MAX = 100;
    MessageWrapper messageWrapper(botWrapper, message);
    std::string text;
    std::string lastItem;
    std::stringstream outStream;
    std::unordered_map<std::string, Random::ret_type> kItemAndPercentMap;
    std::vector<std::string> vec;
    std::set<std::string> set;
    Random::ret_type total = 0;
    using map_t = std::pair<std::string, Random::ret_type>;

    if (!messageWrapper.hasExtraText()) {
        messageWrapper.sendMessageOnExit(
            "Send avaliable conditions sperated by newline");
        return;
    }
    text = messageWrapper.getExtraText();
    // Split string by newline
    boost::split(set, text, isNewline);
    // Pre-reserve memory
    kItemAndPercentMap.reserve(set.size());
    // Can't get possitibities for 1 element
    if (set.size() == 1) {
        messageWrapper.sendMessageOnExit("Give more than 1 choice");
        return;
    }
    // Put it in vector again and shuffle it.
    vec = {set.begin(), set.end()};
    auto [start, end] = std::ranges::remove_if(
        vec, [](auto &&e) { return !isEmptyOrBlank(e); });
    vec = {start, end};
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
    botWrapper->sendReplyMessage(message, outStream.str());
}

DYN_COMMAND_FN(n, module) {
    module.command = "possibility";
    module.description = "Get possibilities";
    module.flags = CommandModule::Flags::None;
    module.fn = COMMAND_HANDLER_NAME(possibility);
    return true;
}