#include <BotReplyMessage.h>
#include <ExtArgs.h>
#include <tgbot/tools/StringTools.h>

#include <TryParseStr.hpp>
#include <functional>
#include <thread>

#include "CommandModule.h"

constexpr int MAX_SPAM_COUNT = 10;
constexpr auto kSpamDelayTime = std::chrono::milliseconds(700);

namespace {

void for_count(int count, std::function<void(void)> callback) {
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
        LOG(WARNING) << "Failed to parse " << data << " as int";
        count = 1;
    }
}
}  // namespace

/**
 * @brief A command module for spamming.
 */
static void SpamCommandFn(const Bot& bot, const Message::Ptr message) {
    std::function<void(void)> fp;
    int count = 0;

    if (message->replyToMessage) {
        if (hasExtArgs(message)) {
            try_parse_spamcnt(parseExtArgs(message), count);
            if (message->replyToMessage->sticker) {
                fp = [&bot, message] {
                    bot_sendSticker(bot, message->chat,
                                    message->replyToMessage->sticker);
                };
            } else if (message->replyToMessage->animation) {
                fp = [&bot, message] {
                    bot_sendAnimation(bot, message->chat,
                                      message->replyToMessage->animation);
                };
            } else if (!message->replyToMessage->text.empty()) {
                fp = [&bot, message] {
                    bot.getApi().sendMessage(message->chat->id,
                                             message->replyToMessage->text);
                };
            } else {
                bot_sendReplyMessage(bot, message,
                                     "Supports sticker/GIF/text for reply to "
                                     "messages, give count");
                return;
            }
        }
    } else if (hasExtArgs(message)) {
        std::string command = parseExtArgs(message);
        std::pair<int, std::string> spamData;
        if (const auto v = StringTools::split(command, ' '); v.size() >= 2) {
            try_parse_spamcnt(v[0], spamData.first);
            spamData.second = command.substr(command.find_first_of(' ') + 1);
            fp = [&bot, message, spamData] {
                bot_sendMessage(bot, message->chat->id, spamData.second);
            };
            count = spamData.first;
        } else {
            bot_sendReplyMessage(bot, message, "Failed to parse spam config");
            return;
        }
    } else {
        bot_sendReplyMessage(bot, message,
                             "Send a pair of spam count and message to spam");
        return;
    }
    for_count(count, fp);
}

struct CommandModule cmd_spam("spam", "Spam a given literal or media",
                              CommandModule::Flags::Enforced, SpamCommandFn);