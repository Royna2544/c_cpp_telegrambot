#include <ExtArgs.h>

#include <thread>

#include "BotReplyMessage.h"
#include "CommandModule.h"
#include "tgbot/tools/StringTools.h"

constexpr int MAX_SPAM_COUNT = 10;
constexpr auto kSpamDelayTime = std::chrono::milliseconds(700);

/**
 * @brief Update the spam count, ensuring it does not exceed the maximum allowed
 * count.
 *
 * @param count The current spam count.
 */
static void updateSpamCount(int& count) {
    if (count > MAX_SPAM_COUNT) {
        count = MAX_SPAM_COUNT;
    }
}

static void SpamCommandFn(const Bot& bot, const Message::Ptr message) {
    if (message->replyToMessage) {
        if (hasExtArgs(message)) {
            int count = stoi(parseExtArgs(message));
            updateSpamCount(count);
            if (message->replyToMessage->sticker) {
                for (int _ = 0; _ < count; _++) {
                    bot_sendSticker(bot, message,
                                    message->replyToMessage->sticker);
                    std::this_thread::sleep_for(kSpamDelayTime);
                }
            } else if (message->replyToMessage->animation) {
                for (int _ = 0; _ < count; _++) {
                    bot.getApi().sendAnimation(
                        message->chat->id,
                        message->replyToMessage->animation->fileId);
                    std::this_thread::sleep_for(kSpamDelayTime);
                }
            } else if (!message->replyToMessage->text.empty()) {
                for (int _ = 0; _ < count; _++) {
                    bot.getApi().sendMessage(message->chat->id,
                                             message->replyToMessage->text);
                    std::this_thread::sleep_for(kSpamDelayTime);
                }
            } else {
                bot_sendReplyMessage(
                    bot, message,
                    "Supports sticker/GIF/text for reply to messages, give count");
            }
        }
    } else if (hasExtArgs(message)) {
        std::string command = parseExtArgs(message);
        std::pair<int, std::string> spamData;
        if (const auto v = StringTools::split(command, ' '); v.size() >= 2) {
            spamData.first = std::stoi(v[0]);
            spamData.second = command.substr(command.find_first_of(' ') + 1);
            updateSpamCount(spamData.first);
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