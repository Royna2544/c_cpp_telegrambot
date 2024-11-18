#include <absl/log/log.h>
#include <tgbot/TgException.h>
#include <trivial_helpers/_std_chrono_templates.h>
#include <trivial_helpers/_tgbot.h>

#include <Authorization.hpp>
#include <algorithm>
#include <global_handlers/SpamBlock.hpp>
#include <iterator>
#include <mutex>
#include <numeric>

#include "Types.h"

void SpamBlockBase::onDetected(ChatId chat, UserId user,
                               std::vector<MessageId> /*messageIds*/) const {
    LOG(INFO) << fmt::format("Spam detected for chat {}, by user {}",
                             chat_map.at(chat), user_map.at(user));
}

void SpamBlockBase::setConfig(Config config) { _config = config; }

void SpamBlockBase::consumeAndDetect() {
    const std::lock_guard<std::mutex> _(mutex);
    for (const auto &[chat, per_chat_map] : chat_messages_data) {
        int count =
            std::accumulate(per_chat_map.begin(), per_chat_map.end(), 0,
                            [](const int /*index*/, const auto &messages) {
                                return messages.second.size();
                            });
        if (count >= sSpamDetectThreshold) {
            const auto &chatPtr = chat_map.at(chat);
            LOG(INFO) << fmt::format(
                "Launching spam detection in {}: Detected {}.", chatPtr, count);
            // Run detection
            for (auto it = per_chat_map.cbegin(); it != per_chat_map.cend();
                 it++) {
                std::vector<MessageId> msgids;
                std::ranges::transform(it->second, std::back_inserter(msgids),
                                       [](const auto &x) { return x.first; });
                if (Matcher::detect<MessageCountMatcher>(it) ||
                    Matcher::detect<SameMessageMatcher>(it)) {
                    onDetected(chat, it->first, msgids);
                }
            }
        }
    }
    chat_messages_data.clear();
}

void SpamBlockBase::addMessage(Message::Ptr message) {
    // Always ignore when spamblock is off
    if (_config == Config::OFF) {
        return;
    }

    // Run possible additional checks
    if (shouldBeSkipped(message)) {
        return;
    }

    // We cares GIF, sticker, text spams only, or if it isn't fowarded msg
    // A required check.
    if ((!message->animation && !message->text && !message->sticker) ||
        message->forwardOrigin) {
        return;
    }

    std::string messageData;
    if (message->text) {
        messageData = *message->text;
    } else if (message->animation){
        messageData = message->animation->fileUniqueId;
    } else if (message->sticker) {
        messageData = message->sticker->fileUniqueId;
    }

    ChatId chatId = message->chat->id;
    UserId userId = message->from->id;
    MessageId messageId = message->messageId;
    {
        const std::lock_guard<std::mutex> _(mutex);
        chat_messages_data[chatId][userId].emplace_back(messageId, messageData);
        chat_map[chatId] = message->chat;
        user_map[userId] = message->from;
    }
}
