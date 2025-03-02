#include <absl/log/log.h>
#include <tgbot/TgException.h>
#include <trivial_helpers/_std_chrono_templates.h>
#include <trivial_helpers/_tgbot.h>

#include <api/Authorization.hpp>
#include <algorithm>
#include <global_handlers/SpamBlock.hpp>
#include <iterator>
#include <mutex>
#include <numeric>

#include "api/typedefs.h"

// Describing a SpamBlockDetector.
class Matcher {
   public:
    virtual ~Matcher() = default;

    // Describes threshold for spam detection.
    // Need to be redeclared in the child scope.
    static constexpr int kThreshold = 0;

    // Declares the name of this Matcher.
    // Need to be redeclared in the child scope.
    static constexpr std::string_view name{};

    // Returns the count of messages per user, that matches the criteria.
    static int count(
        const SpamBlockBase::UserMessagesMap::const_iterator entry) {
        return 0;
    }

    template <std::derived_from<Matcher> T>
    static bool detect(
        const SpamBlockBase::UserMessagesMap::const_iterator entry) {
        static_assert(!T::name.empty(), "Must have a name");
        static_assert(T::kThreshold != 0, "Threshold must be positive");
        int count = T::count(entry);
        if (count >= T::kThreshold) {
            LOG(INFO) << fmt::format(
                "Detected: {} Value {} is over threshold {}", T::name, count,
                T::kThreshold);
        }
        return count >= T::kThreshold;
    }
};

class SameMessageMatcher : public Matcher {
   public:
    static constexpr int kThreshold = 3;
    static constexpr std::string_view name = "SameMessageMatcher";
    static int count(
        const SpamBlockBase::UserMessagesMap::const_iterator entry) {
        std::unordered_map<std::string, int> kSameMessageMap;
        for (const auto &elem : entry->second) {
            const auto &[id, content] = elem;
            if (!kSameMessageMap.contains(content)) {
                kSameMessageMap[content] = 1;
            } else {
                ++kSameMessageMap[content];
            }
        }
        return std::ranges::max_element(kSameMessageMap,
                                        [](const auto &smsg, const auto &rmsg) {
                                            return smsg.second > rmsg.second;
                                        })
            ->second;
    }
};

class MessageCountMatcher : public Matcher {
   public:
    static constexpr int kThreshold = 5;
    static constexpr std::string_view name = "MessageCountMatcher";
    static int count(
        const SpamBlockBase::UserMessagesMap::const_iterator entry) {
        return static_cast<int>(entry->second.size());
    }
};

void SpamBlockBase::onDetected(ChatId chat, UserId user,
                               std::vector<MessageId> /*messageIds*/) const {
    LOG(INFO) << fmt::format("Spam detected for chat {}, by user {}",
                             chat_map.at(chat), user_map.at(user));
}

template <>
struct fmt::formatter<SpamBlockBase::Config> : formatter<std::string_view> {
    // parse is inherited from formatter<string_view>.
    auto format(SpamBlockBase::Config c,
                format_context &ctx) const -> format_context::iterator {
        std::string_view name = "unknown";
        switch (c) {
            case TgBotSocket::data::CtrlSpamBlock::OFF:
                name = "OFF";
                break;
            case TgBotSocket::data::CtrlSpamBlock::LOGGING_ONLY:
                name = "LOGGING_ONLY";
                break;
            case TgBotSocket::data::CtrlSpamBlock::PURGE:
                name = "PURGE";
                break;
            case TgBotSocket::data::CtrlSpamBlock::PURGE_AND_MUTE:
                name = "PURGE_AND_MUTE";
                break;
            default:
                break;
        }
        return formatter<string_view>::format(name, ctx);
    }
};

void SpamBlockBase::setConfig(Config config) {
    LOG(INFO) << fmt::format("Config updated. {} => {}", _config, config);
    _config = config;
}

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
    chat_messages_count = 0;
}

void SpamBlockBase::addMessage(const Message::Ptr &message) {
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
    } else if (message->animation) {
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
        chat_messages_count++;
        onMessageAdded(chat_messages_count);
    }
}
