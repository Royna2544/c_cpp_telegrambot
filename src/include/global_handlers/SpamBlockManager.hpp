#pragma once

#include <ManagedThreads.hpp>

#include "SpamBlock.hpp"

struct SpamBlockManager : SpamBlockBase, ManagedThreadRunnable {
    APPLE_INJECT(SpamBlockManager(TgBotApi::Ptr api, AuthContext *auth));
    ~SpamBlockManager() override = default;

    void runFunction(const std::stop_token &token) override;
    void onDetected(ChatId chat, UserId user,
                    std::vector<MessageId> messageIds) const override;
    // Additional hook for handling messages
    // that should be handled differently
    // (e.g., delete messages, mute users)
    bool shouldBeSkipped(const Message::Ptr &message) const override;

   private:
    constexpr static auto kMuteDuration = std::chrono::minutes(3);
    TgBotApi::Ptr _api;
    AuthContext *_auth;
};