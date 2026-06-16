#include <gtest/gtest.h>

#include <global_handlers/SpamBlock.hpp>
#include <memory>
#include <string>
#include <vector>

namespace {

// Records detections instead of acting on them, so the real detection pipeline
// (addMessage -> consumeAndDetect -> matchers) can be asserted without a bot.
struct RecordingSpamBlock : SpamBlockBase {
    struct Detection {
        ChatId chat;
        UserId user;
    };
    mutable std::vector<Detection> detections;

    void onDetected(ChatId chat, UserId user,
                    std::vector<MessageId> /*ids*/) const override {
        detections.push_back({chat, user});
    }
};

Message::Ptr makeMessage(ChatId chatId, UserId userId, MessageId msgId,
                         const std::string& text) {
    auto message = std::make_shared<Message>();
    message->chat = std::make_shared<Chat>();
    message->chat->id = chatId;
    message->from = std::make_shared<User>();
    (*message->from)->id = userId;
    message->messageId = msgId;
    message->text = text;
    return message;
}

}  // namespace

// One user sending the same text repeatedly: both MessageCountMatcher (>=5) and
// SameMessageMatcher (>=3) fire.
TEST(SpamBlock, DetectsRepeatedMessagesFromOneUser) {
    RecordingSpamBlock sb;
    sb.setConfig(SpamBlockBase::Config::LOGGING_ONLY);
    constexpr ChatId chat = 555;
    constexpr UserId user = 42;
    for (int i = 0; i < 5; ++i) {
        sb.addMessage(makeMessage(chat, user, i + 1, "buy now"));
    }
    sb.consumeAndDetect();
    ASSERT_EQ(sb.detections.size(), 1U);
    EXPECT_EQ(sb.detections[0].chat, chat);
    EXPECT_EQ(sb.detections[0].user, user);
}

// Five distinct messages from five different users: chat-level threshold is met
// so the scan runs, but no single user trips a matcher.
TEST(SpamBlock, IgnoresDistinctMessagesFromDifferentUsers) {
    RecordingSpamBlock sb;
    sb.setConfig(SpamBlockBase::Config::LOGGING_ONLY);
    constexpr ChatId chat = 777;
    for (int i = 0; i < 5; ++i) {
        sb.addMessage(
            makeMessage(chat, 1000 + i, i + 1, "hello " + std::to_string(i)));
    }
    sb.consumeAndDetect();
    EXPECT_TRUE(sb.detections.empty());
}

// Below the chat-level threshold (5), the per-user scan never runs even though
// the three identical messages would otherwise trip SameMessageMatcher.
TEST(SpamBlock, BelowChatThresholdSkipsScan) {
    RecordingSpamBlock sb;
    sb.setConfig(SpamBlockBase::Config::LOGGING_ONLY);
    constexpr ChatId chat = 888;
    for (int i = 0; i < 3; ++i) {
        sb.addMessage(makeMessage(chat, 9, i + 1, "same"));
    }
    sb.consumeAndDetect();
    EXPECT_TRUE(sb.detections.empty());
}
