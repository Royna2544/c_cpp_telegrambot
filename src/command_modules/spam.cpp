#include <BotReplyMessage.h>
#include <ExtArgs.h>
#include <tgbot/tools/StringTools.h>

#include <MessageWrapper.hpp>
#include <TryParseStr.hpp>
#include <boost/algorithm/string/split.hpp>
#include <functional>
#include <thread>

#include "CommandModule.h"
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
static void SpamCommandFn(const Bot& bot, const Message::Ptr& message) {
    std::function<void(void)> fp;
    int count = 0;
    MessageWrapper wrapper(bot, message);
    bool spamable = false;

    if (wrapper.hasReplyToMessage()) {
        if (wrapper.hasExtraText()) {
            const auto chatid = wrapper.getChatId();

            spamable = true;
            try_parse_spamcnt(wrapper.getExtraText(), count);
            wrapper.switchToReplyToMessage();
            if (wrapper.hasSticker()) {
                fp = [&bot, message, &wrapper, chatid] {
                    bot_sendSticker(bot, chatid, wrapper.getSticker());
                };
            } else if (wrapper.hasAnimation()) {
                fp = [&bot, message, chatid, &wrapper] {
                    bot_sendAnimation(bot, chatid, wrapper.getAnimation());
                };
            } else if (wrapper.hasText()) {
                fp = [&bot, message, chatid, &wrapper] {
                    bot_sendMessage(bot, chatid, wrapper.getText());
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
        boost::split(commands, wrapper.getExtraText(), isEmptyChar);
        if (commands.size() == 2) {
            try_parse_spamcnt(commands[0], spamData.first);
            spamData.second = commands[1];
            fp = [&bot, message, spamData] {
                bot_sendMessage(bot, message->chat->id, spamData.second);
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

void loadcmd_spam(CommandModule& module) {
    module.command = "spam";
    module.description = "Spam a given literal or media";
    module.flags = CommandModule::Flags::Enforced;
    module.fn = SpamCommandFn;
}