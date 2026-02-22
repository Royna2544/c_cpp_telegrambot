#include "CommandModulesTest.hpp"
#include "gmock/gmock.h"
#include "tgbot/TgException.h"
#include "tgbot/types/ChatMemberAdministrator.h"
#include "tgbot/types/ChatMemberMember.h"

struct DeleteEchoCommandTest : public CommandTestBase {
    DeleteEchoCommandTest() : CommandTestBase("decho") {}

    void admin() {
        auto adminobj = std::make_shared<TgBot::ChatMemberAdministrator>();
        adminobj->status = TgBot::ChatMemberAdministrator::STATUS;
        adminobj->canDeleteMessages = true;
        ON_CALL(*botApi, getChatMember_impl(TEST_CHAT_ID, TEST_USER_ID))
            .WillByDefault(Return(adminobj));
        ON_CALL(*botApi, getBotUser_impl())
            .WillByDefault(Return(createDefaultUser()));
    }

    void member() {
        auto memberobj = std::make_shared<TgBot::ChatMemberMember>();
        memberobj->status = TgBot::ChatMemberMember::STATUS;
        ON_CALL(*botApi, getChatMember_impl(TEST_CHAT_ID, TEST_USER_ID))
            .WillByDefault(Return(memberobj));
        ON_CALL(*botApi, getBotUser_impl())
            .WillByDefault(Return(createDefaultUser()));
    }
};

TEST_F(DeleteEchoCommandTest, WithRepliedMessageNoArgs) {
    defaultProvidedMessage->replyToMessage = createDefaultMessage();
    defaultProvidedMessage->replyToMessage->text = "This is alex";

    admin();
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
    setCommandExtArgs({"This is alex /del"});

    admin();
    Message::Ptr deletedMessage;
    EXPECT_CALL(*botApi,
                sendMessage_impl(TEST_CHAT_ID, "This is alex /del", _, _, _));
    EXPECT_CALL(*botApi, deleteMessage_impl(_))
        .WillOnce(testing::SaveArg<0>(&deletedMessage));
    execute();
    EXPECT_EQ(deletedMessage, defaultProvidedMessage);
}

TEST_F(DeleteEchoCommandTest, WithRepliedMessageNoArgsCannotDel) {
    defaultProvidedMessage->replyToMessage = createDefaultMessage();
    defaultProvidedMessage->replyToMessage->text = "This is alex";

    admin();
    Message::Ptr deletedMessage;
    // If exception is thrown, the command should catch it and not crash
    EXPECT_CALL(
        *botApi,
        copyMessage_impl(
            TEST_CHAT_ID, defaultProvidedMessage->replyToMessage->messageId,
            createMessageReplyMatcher(defaultProvidedMessage->replyToMessage)));
    // But it should still attempt to delete the message
    EXPECT_CALL(*botApi, deleteMessage_impl(_))
        .WillOnce(
            testing::DoAll(testing::SaveArg<0>(&deletedMessage),
                           testing::Throw(TgBot::TgException(
                               "Cannot delete message",
                               TgBot::TgException::ErrorCode::BadRequest))));
    execute();
    EXPECT_EQ(deletedMessage, defaultProvidedMessage);
}

TEST_F(DeleteEchoCommandTest, WithArgsNoRepliedMessage) {
    setCommandExtArgs({"This is alex /del"});

    admin();
    Message::Ptr deletedMessage;
    EXPECT_CALL(*botApi,
                sendMessage_impl(TEST_CHAT_ID, "This is alex /del", _, _, _));
    EXPECT_CALL(*botApi, deleteMessage_impl(_))
        .WillOnce(testing::SaveArg<0>(&deletedMessage));
    execute();
    EXPECT_EQ(deletedMessage, defaultProvidedMessage);
}

TEST_F(DeleteEchoCommandTest, WithJustAMember) {
    defaultProvidedMessage->replyToMessage = createDefaultMessage();
    defaultProvidedMessage->replyToMessage->text = "This is alex";

    member();
    Message::Ptr deletedMessage;
    // If the bot is not an admin, it should not attempt to delete the message
    EXPECT_CALL(*botApi, deleteMessage_impl(_)).Times(0);
    EXPECT_CALL(*botApi, copyMessage_impl(_, _, _)).Times(0);
    execute();
}
