
#include <memory>
#include "CommandModulesTest.hpp"
#include "gmock/gmock.h"
#include "tgbot/TgException.h"
#include "tgbot/types/Animation.h"

struct DeleteEchoCommandTest : public CommandTestBase {
    DeleteEchoCommandTest() : CommandTestBase("decho") {}
};

TEST_F(DeleteEchoCommandTest, WithRepliedMessageNoArgs) {
    defaultProvidedMessage->replyToMessage = createDefaultMessage();
    defaultProvidedMessage->replyToMessage->text = "This is alex";

    Message::Ptr deletedMessage;
    EXPECT_CALL(*botApi, deleteMessage_impl(_))
        .WillOnce(testing::SaveArg<0>(&deletedMessage));
    EXPECT_CALL(
        *botApi,
        copyMessage_impl(
            TEST_CHAT_ID, defaultProvidedMessage->replyToMessage->messageId,
            createMessageReplyMatcher(defaultProvidedMessage->replyToMessage)));
    execute();
    EXPECT_EQ(deletedMessage, defaultProvidedMessage);
}

TEST_F(DeleteEchoCommandTest, WithRepliedMessageWithArgs) {
    defaultProvidedMessage->replyToMessage = createDefaultMessage();
    defaultProvidedMessage->text = "This is alex /del";

    Message::Ptr deletedMessage;
    EXPECT_CALL(*botApi, deleteMessage_impl(_))
        .WillOnce(testing::SaveArg<0>(&deletedMessage));
    EXPECT_CALL(
        *botApi,
        copyMessage_impl(
            TEST_CHAT_ID, defaultProvidedMessage->replyToMessage->messageId,
            createMessageReplyMatcher(defaultProvidedMessage->replyToMessage)));
    execute();
    EXPECT_EQ(deletedMessage, defaultProvidedMessage);
}

TEST_F(DeleteEchoCommandTest, WithRepliedMessageNoArgsCannotDel) {
    defaultProvidedMessage->replyToMessage = createDefaultMessage();
    defaultProvidedMessage->replyToMessage->text = "This is alex";

    Message::Ptr deletedMessage;
    EXPECT_CALL(*botApi, deleteMessage_impl(_))
        .WillOnce(
            testing::DoAll(testing::SaveArg<0>(&deletedMessage),
                           testing::Throw(TgBot::TgException(
                               "Cannot delete message",
                               TgBot::TgException::ErrorCode::BadRequest))));
    // If exception is thrown, the copyMessage would not be called
    EXPECT_CALL(*botApi, copyMessage_impl(_, _, _)).Times(0);
    execute();
    EXPECT_EQ(deletedMessage, defaultProvidedMessage);
}
