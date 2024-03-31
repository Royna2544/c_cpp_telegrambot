#include <ExtArgs.h>

#include <thread>

#include "BotReplyMessage.h"
#include "CommandModule.h"
#include "tgbot/tools/StringTools.h"

constexpr int MAX_SPAM_COUNT = 10;

static void SpamCommandFn(const Bot &bot, const Message::Ptr message) {
    if (message->replyToMessage) {
        if (message->replyToMessage->sticker && hasExtArgs(message)) {
            int count = stoi(parseExtArgs(message));
            for (int _ = 0; _ < count; _++) {
                bot_sendSticker(bot, message, message->replyToMessage->sticker);
                std::this_thread::sleep_for(std::chrono::milliseconds(700));
            }
        } else {
            bot_sendReplyMessage(bot, message, "Supports sticker for reply to messages, give count");
        }
    } else if (hasExtArgs(message)) {
        std::string command = parseExtArgs(message);
        std::pair<int, std::string> spamData;
        if (const auto v = StringTools::split(command, ' '); v.size() >= 2) {
            spamData.first = std::stoi(v[0]);
            spamData.second = command.substr(command.find_first_of(' ') + 1);
            if (spamData.first > MAX_SPAM_COUNT) {
                spamData.first = MAX_SPAM_COUNT;
            }
            for (int _ = 0; _ < spamData.first; _++) {
                bot_sendMessage(bot, message->chat->id, spamData.second);
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        } else {
            bot_sendReplyMessage(bot, message, "Failed to parse spam config");
        }
    } else {
        bot_sendReplyMessage(bot, message,
                             "Send a pair of spam count and message to spam");
    }
}

struct CommandModule cmd_spam("spam", "Spam a given literal or media",
                              CommandModule::Flags::Enforced, SpamCommandFn);