#pragma once

#include <absl/log/log.h>
#include <fmt/format.h>

#include <algorithm>
#include <api/Authorization.hpp>
#include <api/TgBotApi.hpp>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <mutex>
#include <socket/TgBotSocket_Export.hpp>
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

    SpamBlockBase() = default;
    virtual ~SpamBlockBase() = default;

    // virtual function, hooks before the message is added.
    // Returns true if the message should be skipped, false otherwise.
    // Dummy version: Returns false.
    virtual bool shouldBeSkipped(const Message::Ptr& /*msg*/) const {
        return false;
    }

    // virtual function, called when the message is added to the map
    virtual void onMessageAdded(const size_t count) {
        // Default: do nothing
    }

    // Set the SpamBlock config. Based on SpamBlockBase::Config.
    virtual void setConfig(Config config);
    // Get the SpamBlock config. Based on SpamBlockBase::Config.
    Config getConfig() const { return _config; }

    // Function called when the SpamBlock framework detects spamming user.
    // Arguments passed: ChatId, UserId, Offending messageIds
    virtual void onDetected(ChatId chat, UserId user,
                            std::vector<MessageId> messageIds) const;

    // Add a message to the buffer.
    void addMessage(const Message::Ptr& message);

    // Run the SpamBlock framework.
    void consumeAndDetect();

   private:
    Config _config = Config::PURGE;

    std::unordered_map<ChatId, UserMessagesMap> chat_messages_data;
    size_t chat_messages_count;

    // Cache these for easy lookup
    std::unordered_map<ChatId, Chat::Ptr> chat_map;
    std::unordered_map<UserId, User::Ptr> user_map;

    mutable std::mutex mutex;  // Protect above maps
};
