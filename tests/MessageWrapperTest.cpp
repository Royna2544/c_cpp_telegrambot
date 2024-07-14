#include <gtest/gtest.h>
#include <tgbot/tgbot.h>
#include <TgBotWrapper.hpp>

using namespace TgBot;

// Mock message creation helper
Message::Ptr createMockMessage(ChatId chatId, const std::string& text, bool hasReply = false) {
    auto message = std::make_shared<Message>();
    auto chat = std::make_shared<Chat>();
    chat->id = chatId;
    message->chat = chat;
    message->text = text;

    if (hasReply) {
        auto replyMessage = std::make_shared<Message>();
        replyMessage->text = "Reply message";
        message->replyToMessage = replyMessage;
    }
    return message;
}

// Test suite for MessageWrapperLimited
class MessageWrapperLimitedTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up any shared resources for the tests
    }

    void TearDown() override {
        // Clean up any shared resources
    }
};

TEST_F(MessageWrapperLimitedTest, GetChatId) {
    auto message = createMockMessage(12345, "Hello, World!");
    MessageWrapperLimited wrapper(message);
    EXPECT_EQ(wrapper.getChatId(), 12345);
}

TEST_F(MessageWrapperLimitedTest, HasReplyToMessage) {
    auto message = createMockMessage(12345, "Hello, World!", true);
    MessageWrapperLimited wrapper(message);
    EXPECT_TRUE(wrapper.hasReplyToMessage());
}

TEST_F(MessageWrapperLimitedTest, SwitchToReplyToMessage) {
    auto message = createMockMessage(12345, "Hello, World!", true);
    MessageWrapperLimited wrapper(message);
    EXPECT_TRUE(wrapper.switchToReplyToMessage());
    EXPECT_EQ(wrapper.getText(), "Reply message");
}

TEST_F(MessageWrapperLimitedTest, SwitchToParent) {
    auto message = createMockMessage(12345, "Hello, World!", true);
    MessageWrapperLimited wrapper(message);
    wrapper.switchToReplyToMessage();
    wrapper.switchToParent();
    EXPECT_EQ(wrapper.getText(), "Hello, World!");
}

TEST_F(MessageWrapperLimitedTest, HasExtraText) {
    auto message = createMockMessage(12345, "/Hello, World extra text");
    MessageWrapperLimited wrapper(message);
    EXPECT_TRUE(wrapper.hasExtraText());
}

TEST_F(MessageWrapperLimitedTest, GetExtraText) {
    auto message = createMockMessage(12345, "/Hello, World extra text");
    MessageWrapperLimited wrapper(message);
    EXPECT_EQ(wrapper.getExtraText(), "World extra text");
}

TEST_F(MessageWrapperLimitedTest, HasSticker) {
    auto message = createMockMessage(12345, "Hello, World");
    message->sticker = std::make_shared<Sticker>();
    MessageWrapperLimited wrapper(message);
    EXPECT_TRUE(wrapper.hasSticker());
}

TEST_F(MessageWrapperLimitedTest, GetSticker) {
    auto message = createMockMessage(12345, "Hello, World");
    auto sticker = std::make_shared<Sticker>();
    message->sticker = sticker;
    MessageWrapperLimited wrapper(message);
    EXPECT_EQ(wrapper.getSticker(), sticker);
}

TEST_F(MessageWrapperLimitedTest, HasAnimation) {
    auto message = createMockMessage(12345, "Hello, World");
    message->animation = std::make_shared<Animation>();
    MessageWrapperLimited wrapper(message);
    EXPECT_TRUE(wrapper.hasAnimation());
}

TEST_F(MessageWrapperLimitedTest, GetAnimation) {
    auto message = createMockMessage(12345, "Hello, World");
    auto animation = std::make_shared<Animation>();
    message->animation = animation;
    MessageWrapperLimited wrapper(message);
    EXPECT_EQ(wrapper.getAnimation(), animation);
}

TEST_F(MessageWrapperLimitedTest, HasText) {
    auto message = createMockMessage(12345, "Hello, World");
    MessageWrapperLimited wrapper(message);
    EXPECT_TRUE(wrapper.hasText());
}

TEST_F(MessageWrapperLimitedTest, GetText) {
    auto message = createMockMessage(12345, "Hello, World");
    MessageWrapperLimited wrapper(message);
    EXPECT_EQ(wrapper.getText(), "Hello, World");
}

TEST_F(MessageWrapperLimitedTest, HasUser) {
    auto message = createMockMessage(12345, "Hello, World");
    message->from = std::make_shared<User>();
    MessageWrapperLimited wrapper(message);
    EXPECT_TRUE(wrapper.hasUser());
}

TEST_F(MessageWrapperLimitedTest, GetUser) {
    auto message = createMockMessage(12345, "Hello, World");
    auto user = std::make_shared<User>();
    message->from = user;
    MessageWrapperLimited wrapper(message);
    EXPECT_EQ(wrapper.getUser(), user);
}
