#include "CommandModulesTest.hpp"
#include "gmock/gmock.h"
#include "tgbot/TgException.h"
#include "tgbot/types/PhotoSize.h"

struct FileIdCommandTest : public CommandTestBase {
    FileIdCommandTest() : CommandTestBase("fileid") {}
};

TEST_F(FileIdCommandTest, WithRepliedMedia) {
    defaultProvidedMessage->replyToMessage = createDefaultMessage();
    defaultProvidedMessage->replyToMessage->photo.push_back(
        TgBot::PhotoSize ::Ptr(new TgBot::PhotoSize()));
    defaultProvidedMessage->replyToMessage->photo[0]->fileId = TEST_MEDIA_ID;
    defaultProvidedMessage->replyToMessage->photo[0]->fileUniqueId =
        TEST_MEDIA_UNIQUEID;
    std::string savedMessage;
    EXPECT_CALL(*botApi, sendMessage_impl(TEST_CHAT_ID, _, _, _, _))
        .WillOnce(testing::DoAll(testing::SaveArg<1>(&savedMessage),
                                 testing::Return(createDefaultMessage())));
    execute();
    EXPECT_TRUE(savedMessage.find(TEST_MEDIA_ID) != std::string::npos);
    EXPECT_TRUE(savedMessage.find(TEST_MEDIA_UNIQUEID) != std::string::npos);
}

TEST_F(FileIdCommandTest, WithRepliedNonMedia) {
    defaultProvidedMessage->replyToMessage = createDefaultMessage();
    defaultProvidedMessage->replyToMessage->text = "This is alex";
    std::string savedMessage;
    EXPECT_CALL(*botApi, sendMessage_impl(TEST_CHAT_ID, _, _, _, _))
        .WillOnce(testing::DoAll(testing::SaveArg<1>(&savedMessage),
                                 testing::Return(createDefaultMessage())));
    execute();
    EXPECT_FALSE(savedMessage.find(TEST_MEDIA_ID) != std::string::npos);
    EXPECT_FALSE(savedMessage.find(TEST_MEDIA_UNIQUEID) != std::string::npos);
}