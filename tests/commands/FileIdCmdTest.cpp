#include "CommandModulesTest.hpp"

using testing::HasSubstr;

class FileIdCommandTest : public CommandTestBase {
   public:
    FileIdCommandTest() : CommandTestBase("fileid") {}
    ~FileIdCommandTest() override = default;
};

TEST_F(FileIdCommandTest, NoReplyMessage) {
    setCommandExtArgs();
    
    ON_CALL(strings, get(Strings::REPLY_TO_A_MEDIA))
        .WillByDefault(Return("Reply to a media"));
    
    willSendReplyMessage(HasSubstr("Reply to a media"));
    execute();
}

TEST_F(FileIdCommandTest, ReplyToSticker) {
    setCommandExtArgs();
    
    defaultProvidedMessage->replyToMessage = createDefaultMessage();
    auto sticker = std::make_shared<TgBot::Sticker>();
    sticker->fileId = "test_file_id";
    sticker->fileUniqueId = "test_unique_id";
    defaultProvidedMessage->replyToMessage->sticker = sticker;
    
    willSendReplyMessage<TgBotApi::ParseMode::Markdown>(
        HasSubstr("test_file_id"));
    execute();
}

TEST_F(FileIdCommandTest, ReplyToAnimation) {
    setCommandExtArgs();
    
    defaultProvidedMessage->replyToMessage = createDefaultMessage();
    auto animation = std::make_shared<TgBot::Animation>();
    animation->fileId = "anim_file_id";
    animation->fileUniqueId = "anim_unique_id";
    defaultProvidedMessage->replyToMessage->animation = animation;
    
    willSendReplyMessage<TgBotApi::ParseMode::Markdown>(
        HasSubstr("anim_file_id"));
    execute();
}

TEST_F(FileIdCommandTest, ReplyToPhoto) {
    setCommandExtArgs();
    
    defaultProvidedMessage->replyToMessage = createDefaultMessage();
    auto photo = std::make_shared<TgBot::PhotoSize>();
    photo->fileId = "photo_file_id";
    photo->fileUniqueId = "photo_unique_id";
    defaultProvidedMessage->replyToMessage->photo = {photo};
    
    willSendReplyMessage<TgBotApi::ParseMode::Markdown>(
        HasSubstr("photo_file_id"));
    execute();
}
