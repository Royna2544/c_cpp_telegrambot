
#include <tgbot/tools/StringTools.h>

#include <TgBotWrapper.hpp>
#include <TryParseStr.hpp>
#include <boost/algorithm/string/split.hpp>
#include <functional>
#include <thread>

#include "StringToolsExt.hpp"

constexpr int MAX_SPAM_COUNT = 10;
constexpr auto kSpamDelayTime = std::chrono::milliseconds(700);

namespace {

void for_count(int count, const std::function<void(void)>& callback) {
    if (count > MAX_SPAM_COUNT) {
        count = MAX_SPAM_COUNT;
    }
    for (int i = 0; i < count; ++i) {
        callback();
        std::this_thread::sleep_for(kSpamDelayTime);
    }
}
void try_parse_spamcnt(const std::string& data, int& count) {
    if (count > MAX_SPAM_COUNT) {
        count = MAX_SPAM_COUNT;
    }
    if (!try_parse(data, &count)) {
        LOG(WARNING) << "Failed to parse " << std::quoted(data) << " as int";
        count = 1;
    }
}
}  // namespace

/**
 * @brief A command module for spamming.
 */
DECLARE_COMMAND_HANDLER(spam, bot, message) {
    std::function<void(void)> fp;
    int count = 0;
    MessageWrapper wrapper(message);
    bool spamable = false;

    if (wrapper.hasReplyToMessage()) {
        if (wrapper.hasExtraText()) {
            const auto chatid = wrapper.getChatId();

            spamable = true;
            try_parse_spamcnt(wrapper.getExtraText(), count);
            wrapper.switchToReplyToMessage();
            if (wrapper.hasSticker()) {
                fp = [bot, message, &wrapper, chatid] {
                    bot->sendSticker(chatid, MediaIds(wrapper.getSticker()));
                };
            } else if (wrapper.hasAnimation()) {
                fp = [bot, message, chatid, &wrapper] {
                    bot->sendAnimation(chatid,
                                       MediaIds(wrapper.getAnimation()));
                };
            } else if (wrapper.hasText()) {
                fp = [bot, message, chatid, &wrapper] {
                    bot->sendMessage(chatid, wrapper.getText());
                };
            } else {
                wrapper.sendMessageOnExit(
                    "Supports sticker/GIF/text for reply to "
                    "messages, give count");
                spamable = false;
            }
        }
    } else if (wrapper.hasExtraText()) {
        std::vector<std::string> commands;
        std::pair<int, std::string> spamData;
        splitWithSpaces(wrapper.getExtraText(), commands);
        if (commands.size() == 2) {
            try_parse_spamcnt(commands[0], spamData.first);
            spamData.second = commands[1];
            fp = [bot, message, spamData] {
                bot->sendMessage(message->chat->id, spamData.second);
            };
            count = spamData.first;
            spamable = true;
        } else {
            wrapper.sendMessageOnExit("Invalid argument size for spam config");
        }
    } else {
        wrapper.sendMessageOnExit(
            "Send a pair of spam count and message to spam");
    }
    if (spamable) {
        for_count(count, fp);
    }
}

DYN_COMMAND_FN(n, module) {
    module.command = "spam";
    module.description = "Spam a given literal or media";
    module.flags = CommandModule::Flags::Enforced;
    module.fn = COMMAND_HANDLER_NAME(spam);
    return true;
}