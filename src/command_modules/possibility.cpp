#include <ExtArgs.h>
#include <StringToolsExt.h>
#include <random/RandomNumberGenerator.h>

#include <set>

#include "CommandModule.h"

static void PossibilityCommandFn(const Bot &bot, const Message::Ptr message) {
    constexpr int PERCENT_MAX = 100;
    if (!hasExtArgs(message)) {
        bot_sendReplyMessage(bot, message,
                             "Send avaliable conditions sperated by newline");
        return;
    }
    std::string text;
    parseExtArgs(message, text);
    std::stringstream ss(text), out;
    std::string last;
    std::unordered_map<std::string, int> map;
    std::vector<std::string> vec;
    std::set<std::string> set;

    splitAndClean(text, vec);
    set = {vec.begin(), vec.end()};
    if (set.size() != vec.size()) {
        out << "(Warning: Removed " << vec.size() - set.size() << " duplicates)"
            << std::endl
            << std::endl;
        LOG(WARNING) << "Contains duplicates! removed"
                     << vec.size() - set.size() << " duplicates";
    }
    map.reserve(set.size());
    if (set.size() == 1) {
        bot_sendReplyMessage(bot, message, "Give more than 1 choice");
        return;
    }
    vec = {set.begin(), set.end()};
    shuffleStringArray(vec);
    out << "Total " << vec.size() << " items" << std::endl;
    last = vec.back();
    vec.pop_back();
    int total = 0;
    for (const auto &cond : vec) {
        random_return_type thisper = 0;
        if (total < PERCENT_MAX) {
            thisper = genRandomNumber(PERCENT_MAX - total);
            if (total + thisper >= PERCENT_MAX) {
                thisper = PERCENT_MAX - total;
            }
        }
        map[cond] = thisper;
        total += thisper;
    }
    // Nonetheless of total being 100 or whatever
    map[last] = PERCENT_MAX - total;
    using map_t = std::pair<std::string, int>;
    std::vector<map_t> elem(map.begin(), map.end());
    std::sort(elem.begin(), elem.end(),
              [](const map_t &map1, const map_t &map2) {
                  if (map1.second != map2.second) {
                      return map1.second > map2.second;
                  }
                  return map1.first < map2.first;
              });
    for (const map_t &m : elem) {
        out << m.first << " : " << m.second << "%" << std::endl;
    }
    bot_sendReplyMessage(bot, message, out.str());
}

struct CommandModule cmd_possibility("possibility", "Get possibilities",
                                     CommandModule::Flags::None,
                                     PossibilityCommandFn);