#include <absl/log/log.h>
#include <tgbot/TgException.h>
#include <trivial_helpers/_std_chrono_templates.h>
#include <trivial_helpers/_tgbot.h>

#include <algorithm>
#include <api/AuthContext.hpp>
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
        for (const auto& elem : entry->second) {
            const auto& [id, content] = elem;
            ++kSameMessageMap[content];
        }
        return std::ranges::max_element(kSameMessageMap,
                                        [](const auto& smsg, const auto& rmsg) {
                                            return smsg.second < rmsg.second;
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
    const std::lock_guard lock(mutex);
    const auto chatIt = chat_map.find(chat);
    const auto userIt = user_map.find(user);
    LOG(INFO) << fmt::format(
        "Spam detected for chat {}, by user {}",
        chatIt != chat_map.end() ? fmt::format("{}", chatIt->second)
                                 : std::to_string(chat),
        userIt != user_map.end() ? fmt::format("{}", userIt->second)
                                 : std::to_string(user));
}

template <>
struct fmt::formatter<SpamBlockBase::Config> : formatter<std::string_view> {
    // parse is inherited from formatter<string_view>.
    auto format(SpamBlockBase::Config c, format_context& ctx) const
        -> format_context::iterator {
        std::string_view name = "unknown";
        switch (c) {
            case SpamBlockBase::Config::OFF:
                name = "OFF";
                break;
            case SpamBlockBase::Config::LOGGING_ONLY:
                name = "LOGGING_ONLY";
                break;
            case SpamBlockBase::Config::PURGE:
                name = "PURGE";
                break;
            case SpamBlockBase::Config::PURGE_AND_MUTE:
                name = "PURGE_AND_MUTE";
                break;
            default:
                break;
        }
        return formatter<string_view>::format(name, ctx);
    }
};

void SpamBlockBase::setConfig(Config config) {
    const auto previous = _config.exchange(config, std::memory_order_acq_rel);
    LOG(INFO) << fmt::format("Config updated. {} => {}", previous, config);
}

void SpamBlockBase::consumeAndDetect() {
    // Move the current batch out under the data lock, then perform matching
    // and Telegram actions without it. A slow delete/mute request must never
    // block the polling thread from recording or dispatching new commands.
    decltype(chat_messages_data) pending;
    {
        const std::lock_guard lock(mutex);
        pending.swap(chat_messages_data);
        chat_messages_count.store(0, std::memory_order_release);
    }
    for (const auto& [chat, per_chat_map] : pending) {
        int count = std::accumulate(
            per_chat_map.begin(), per_chat_map.end(), 0,
            [](const int index, const auto& messages) {
                return index + static_cast<int>(messages.second.size());
            });
        if (count >= sSpamDetectThreshold) {
            LOG(INFO) << fmt::format(
                "Launching spam detection in chat {}: Detected {}.", chat,
                count);
            // Run detection
            for (auto it = per_chat_map.cbegin(); it != per_chat_map.cend();
                 it++) {
                std::vector<MessageId> msgids;
                std::ranges::transform(it->second, std::back_inserter(msgids),
                                       [](const auto& x) { return x.first; });
                if (Matcher::detect<MessageCountMatcher>(it) ||
                    Matcher::detect<SameMessageMatcher>(it)) {
                    try {
                        onDetected(chat, it->first, msgids);
                    } catch (const std::exception& error) {
                        LOG(ERROR) << "Spam action failed: " << error.what();
                    } catch (...) {
                        LOG(ERROR) << "Spam action failed: unknown exception";
                    }
                }
            }
        }
    }
}

void SpamBlockBase::addMessage(const Message::Ptr& message) {
    // Always ignore when spamblock is off
    if (getConfig() == Config::OFF) {
        return;
    }

    if (!message || !message->chat || !message->from) {
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
        messageData = (*message->animation)->fileUniqueId;
    } else if (message->sticker) {
        messageData = (*message->sticker)->fileUniqueId;
    }

    ChatId chatId = message->chat->id;
    UserId userId = (*message->from)->id;
    MessageId messageId = message->messageId;
    std::size_t count = 0;
    {
        const std::lock_guard<std::mutex> _(mutex);
        chat_messages_data[chatId][userId].emplace_back(messageId, messageData);
        chat_map[chatId] = message->chat;
        user_map[userId] = *message->from;
        count = chat_messages_count.fetch_add(1, std::memory_order_acq_rel) + 1;
    }
    onMessageAdded(count);
}
