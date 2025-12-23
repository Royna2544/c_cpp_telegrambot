#include "CommandModulesTest.hpp"

using testing::_;

class SpamCommandTest : public CommandTestBase {
   public:
    SpamCommandTest() : CommandTestBase("spam") {}
    ~SpamCommandTest() override = default;
};

TEST_F(SpamCommandTest, NoReplyNoArgs) {
    setCommandExtArgs();
    
    willSendReplyMessage(_);
    execute();
}

TEST_F(SpamCommandTest, InvalidArgSize) {
    setCommandExtArgs({"onearg"});
    
    willSendReplyMessage(_);
    execute();
}

TEST_F(SpamCommandTest, ValidTextSpam) {
    setCommandExtArgs({"3 hello"});
    
    // Expect 3 sendMessage calls
    EXPECT_CALL(*botApi, sendMessage_impl(TEST_CHAT_ID, "hello", _, _, _))
        .Times(3);
    
    execute();
}

TEST_F(SpamCommandTest, ReplyToTextMessage) {
    setCommandExtArgs({"2"});
    
    defaultProvidedMessage->replyToMessage = createDefaultMessage();
    defaultProvidedMessage->replyToMessage->text = "spam this";
    
    // Expect 2 sendMessage calls
    EXPECT_CALL(*botApi, sendMessage_impl(TEST_CHAT_ID, "spam this", _, _, _))
        .Times(2);
    
    execute();
}

TEST_F(SpamCommandTest, ReplyToSticker) {
    setCommandExtArgs({"2"});
    
    defaultProvidedMessage->replyToMessage = createDefaultMessage();
    auto sticker = std::make_shared<TgBot::Sticker>();
    sticker->fileId = "sticker_id";
    defaultProvidedMessage->replyToMessage->sticker = sticker;
    
    // Expect 2 sendSticker calls
    EXPECT_CALL(*botApi, sendSticker_impl(TEST_CHAT_ID, _, _, _))
        .Times(2);
    
    execute();
}

TEST_F(SpamCommandTest, ReplyToAnimation) {
    setCommandExtArgs({"2"});
    
    defaultProvidedMessage->replyToMessage = createDefaultMessage();
    auto animation = std::make_shared<TgBot::Animation>();
    animation->fileId = "anim_id";
    defaultProvidedMessage->replyToMessage->animation = animation;
    
    // Expect 2 sendAnimation calls
    EXPECT_CALL(*botApi, sendAnimation_impl(TEST_CHAT_ID, _, _, _, _, _))
        .Times(2);
    
    execute();
}
