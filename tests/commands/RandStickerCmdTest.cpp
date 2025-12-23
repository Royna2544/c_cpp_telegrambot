#include "CommandModulesTest.hpp"

using testing::_;
using testing::Return;

class RandStickerCommandTest : public CommandTestBase {
   public:
    RandStickerCommandTest() : CommandTestBase("randsticker") {
        ON_CALL(strings, get(Strings::REPLY_TO_A_STICKER))
            .WillByDefault(Return("Reply to a sticker"));
    }
    ~RandStickerCommandTest() override = default;
};

TEST_F(RandStickerCommandTest, NoReplyMessage) {
    setCommandExtArgs();
    
    willSendReplyMessage(_);
    execute();
}

TEST_F(RandStickerCommandTest, ReplyToStickerWithSet) {
    setCommandExtArgs();
    
    defaultProvidedMessage->replyToMessage = createDefaultMessage();
    auto sticker = std::make_shared<TgBot::Sticker>();
    sticker->fileId = "test_sticker_id";
    sticker->setName = "test_pack";
    defaultProvidedMessage->replyToMessage->sticker = sticker;
    
    // Create a mock sticker set
    auto stickerSet = std::make_shared<TgBot::StickerSet>();
    stickerSet->title = "Test Pack";
    auto sticker1 = std::make_shared<TgBot::Sticker>();
    sticker1->fileId = "sticker1_id";
    sticker1->emoji = "😀";
    auto sticker2 = std::make_shared<TgBot::Sticker>();
    sticker2->fileId = "sticker2_id";
    sticker2->emoji = "😂";
    stickerSet->stickers = {sticker1, sticker2};
    
    EXPECT_CALL(*botApi, getStickerSet_impl("test_pack"))
        .WillOnce(Return(stickerSet));
    
    ON_CALL(*random, generate(_))
        .WillByDefault(Return(0));
    
    EXPECT_CALL(*botApi, sendSticker_impl(TEST_CHAT_ID, _, _));
    EXPECT_CALL(*botApi, sendMessage_impl(TEST_CHAT_ID, _, _, _, _));
    
    execute();
}
