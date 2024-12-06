#pragma once

#include <ManagedThreads.hpp>

#include "SpamBlock.hpp"

struct SpamBlockManager : SpamBlockBase, ThreadRunner {
    APPLE_INJECT(SpamBlockManager(TgBotApi::Ptr api, AuthContext *auth));
    ~SpamBlockManager() override = default;

    void runFunction(const std::stop_token &token) override;
    void onDetected(ChatId chat, UserId user,
                    std::vector<MessageId> messageIds) const override;
    void onMessageAdded(const size_t count) override;

    // Additional hook for handling messages
    // that should be handled differently
    // (e.g., delete messages, mute users)
    bool shouldBeSkipped(const Message::Ptr &message) const override;

    void onPreStop() override;

   private:
    constexpr static auto kMuteDuration = std::chrono::minutes(3);
    TgBotApi::Ptr _api;
    AuthContext *_auth;
    std::condition_variable condvar;
    std::mutex mutex;

    // Start next iteration when the message in buffer is bigger than sImmediateStartThreshold
    constexpr static int sImmediateStartThreshold = 10;
};