#pragma once

#include <absl/log/log.h>
#include <fmt/format.h>

#include <Authorization.hpp>
#include <algorithm>
#include <api/TgBotApi.hpp>
#include <chrono>
#include <concepts>
#include <mutex>
#include <socket/include/TgBotSocket_Export.hpp>
#include <unordered_map>

using TgBot::Chat;
using TgBot::Message;
using TgBot::User;

struct SpamBlockBase {
    // This is a per-chat map, containing UserId and the vector of pair of
    // messageid-messagecontent
    using UserMessagesMap =
        std::unordered_map<UserId,
                           std::vector<std::pair<MessageId, std::string>>>;
    using Config = TgBotSocket::data::CtrlSpamBlock;

    // Triggered when a chat has more than sSpamDetectThreshold messages
    // In sSpamDetectDelay delay.
    constexpr static int sSpamDetectThreshold = 5;
    constexpr static std::chrono::seconds sSpamDetectDelay{10};

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
        static int count(const UserMessagesMap::const_iterator entry) {
            return 0;
        }

        template <std::derived_from<Matcher> T>
        static bool detect(const UserMessagesMap::const_iterator entry) {
            static_assert(!T::name.empty(), "Must have a name");
            static_assert(T::kThreshold != 0, "Threshold must be positive");
            int count = T::count(entry);
            if (count >= T::kThreshold) {
                LOG(INFO) << fmt::format(
                    "Detected: {} Value {} is over threshold {}", T::name,
                    count, T::kThreshold);
            }
            return count >= T::kThreshold;
        }
    };

    class SameMessageMatcher : public Matcher {
       public:
        static constexpr int kThreshold = 3;
        static constexpr std::string_view name = "SameMessageMatcher";
        static int count(const UserMessagesMap::const_iterator entry) {
            std::unordered_map<std::string, int> kSameMessageMap;
            for (const auto &elem : entry->second) {
                const auto &[id, content] = elem;
                if (!kSameMessageMap.contains(content)) {
                    kSameMessageMap[content] = 1;
                } else {
                    ++kSameMessageMap[content];
                }
            }
            return std::ranges::max_element(
                       kSameMessageMap,
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
        static int count(const UserMessagesMap::const_iterator entry) {
            return static_cast<int>(entry->second.size());
        }
    };

    SpamBlockBase() = default;
    virtual ~SpamBlockBase() = default;

    // Pure virtual function, hooks before the message is added.
    // Returns true if the message should be skipped, false otherwise.
    // Dummy version: Returns false.
    virtual bool shouldBeSkipped(const Message::Ptr & /*msg*/) const {
        return false;
    }

    // Set the SpamBlock config. Based on SpamBlockBase::Config.
    virtual void setConfig(Config config);

    // Function called when the SpamBlock framework detects spamming user.
    // Arguments passed: ChatId, UserId, Offending messageIds
    virtual void onDetected(ChatId chat, UserId user,
                            std::vector<MessageId> messageIds) const;

    // Add a message to the buffer.
    void addMessage(Message::Ptr message);

    // Run the SpamBlock framework.
    void consumeAndDetect();

   private:
    std::unordered_map<ChatId, UserMessagesMap> chat_messages_data;

    // Cache these for easy lookup
    std::unordered_map<ChatId, Chat::Ptr> chat_map;
    std::unordered_map<UserId, User::Ptr> user_map;

    mutable std::mutex mutex;  // Protect above maps
   protected:
    Config _config = Config::PURGE;
};
