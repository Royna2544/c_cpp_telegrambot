#pragma once

#include <absl/log/log.h>
#include <fmt/format.h>

#include <algorithm>
#include <api/AuthContext.hpp>
#include <api/TgBotApi.hpp>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <mutex>
#include <socket/api/DataStructures.hpp>
#include <unordered_map>

struct SpamBlockBase {
    // This is a per-chat map, containing api::types::User::id_type and the vector of pair of
    // messageid-messagecontent
    using UserMessagesMap =
        std::unordered_map<api::types::User::id_type,
                           std::vector<std::pair<api::types::Message::messageId_type, std::string>>>;
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
    virtual bool shouldBeSkipped(const api::types::Message& /*msg*/) const {
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
    // Arguments passed: api::types::Chat::id_type, api::types::User::id_type, Offending messageIds
    virtual void onDetected(api::types::Chat::id_type chat, api::types::User::id_type user,
                            std::vector<api::types::Message::messageId_type> messageIds) const;

    // Add a message to the buffer.
    void addMessage(const api::types::Message& message);

    // Run the SpamBlock framework.
    void consumeAndDetect();

   private:
    Config _config = Config::PURGE;

    std::unordered_map<api::types::Chat::id_type, UserMessagesMap> chat_messages_data;
    size_t chat_messages_count;

    // Cache these for easy lookup
    std::unordered_map<api::types::Chat::id_type, api::types::Chat> chat_map;
    std::unordered_map<api::types::User::id_type, api::types::User> user_map;

    mutable std::mutex mutex;  // Protect above maps
};
