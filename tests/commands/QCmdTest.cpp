#include <gmock/gmock.h>
#include <tgbot/types/PhotoSize.h>

#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <variant>

#include "CommandModulesTest.hpp"

namespace {

constexpr std::uint8_t kPng[] = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
    0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
    0x08, 0x06, 0x00, 0x00, 0x00, 0x1f, 0x15, 0xc4, 0x89, 0x00, 0x00, 0x00,
    0x0d, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x63, 0xf8, 0xcf, 0xc0, 0xf0,
    0x1f, 0x00, 0x05, 0x00, 0x01, 0xff, 0x89, 0x99, 0x3d, 0x1d, 0x00, 0x00,
    0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82,
};

class QCommandTest : public CommandTestBase {
   public:
    QCommandTest() : CommandTestBase("q") {}

   protected:
    void SetUp() override {
        CommandTestBase::SetUp();
        ON_CALL(strings, get(testing::_))
            .WillByDefault(testing::Invoke([](Strings value) {
                static constexpr std::string_view failure =
                    "quote generation failed";
                static constexpr std::string_view invalidId = "invalid id: {}";
                static constexpr std::string_view unsupported =
                    "unsupported origin: {}";
                switch (value) {
                    case Strings::QUOTE_INVALID_ID:
                        return invalidId;
                    case Strings::QUOTE_UNSUPPORTED_ORIGIN_TYPE:
                        return unsupported;
                    default:
                        return failure;
                }
            }));
        ON_CALL(*botApi, getUserProfilePhotos_impl(testing::_))
            .WillByDefault(testing::Return(nullptr));
        setCommandExtArgs();
        defaultProvidedMessage->replyToMessage = createDefaultMessage();
    }

    void expectWebPSticker() {
        stickerSent = false;
        EXPECT_CALL(*botApi,
                    sendSticker_impl(
                        TEST_CHAT_ID,
                        testing::Truly([](const TgBotApi::FileOrString& value) {
                            const auto* input =
                                std::get_if<InputFile::Ptr>(&value);
                            if (!input || !*input)
                                return false;
                            const auto& file = **input;
                            return file.mimeType == "image/webp" &&
                                   file.fileName == "quote.webp" &&
                                   file.data.size() >= 12 &&
                                   file.data.compare(0, 4, "RIFF") == 0 &&
                                   file.data.compare(8, 4, "WEBP") == 0 &&
                                   file.data.size() <= 512U * 1024U;
                        }),
                        testing::_))
            .WillOnce(testing::Invoke([this](auto&&...) {
                stickerSent = true;
                return createDefaultMessage();
            }));
    }

    bool stickerSent = false;
};

TEST_F(QCommandTest, RendersRepliedInternationalTextLocally) {
    (*defaultProvidedMessage->replyToMessage)->text =
        "한국어 😀 العربية native quote";
    expectWebPSticker();

    execute();
    EXPECT_TRUE(stickerSent);
}

TEST_F(QCommandTest, RejectsMalformedIdOverrideExactly) {
    (*defaultProvidedMessage->replyToMessage)->text = "quoted text";
    setCommandExtArgs({"id=123junk"});
    EXPECT_CALL(
        *botApi,
        sendMessage_impl(TEST_CHAT_ID, testing::HasSubstr("invalid numeric ID"),
                         testing::_, testing::_, testing::_))
        .WillOnce(testing::Return(createDefaultMessage()));
    EXPECT_CALL(*botApi, getChat_impl(testing::_)).Times(0);

    execute();
}

TEST_F(QCommandTest, RejectsSenderlessReplyWithoutDereference) {
    auto& reply = *defaultProvidedMessage->replyToMessage;
    reply->text = "senderless";
    reply->from.reset();
    reply->senderChat.reset();
    EXPECT_CALL(*botApi,
                sendMessage_impl(TEST_CHAT_ID,
                                 testing::HasSubstr("sender-less message"),
                                 testing::_, testing::_, testing::_))
        .WillOnce(testing::Return(createDefaultMessage()));

    execute();
}

TEST_F(QCommandTest, ExtractsAndDownloadsLargestPhoto) {
    auto& reply = *defaultProvidedMessage->replyToMessage;
    reply->caption = "photo caption";
    reply->photo.emplace();
    auto small = std::make_shared<TgBot::PhotoSize>();
    small->fileId = "small";
    small->width = 32;
    small->height = 32;
    auto large = std::make_shared<TgBot::PhotoSize>();
    large->fileId = "large";
    large->width = 640;
    large->height = 480;
    reply->photo->push_back(std::move(small));
    reply->photo->push_back(std::move(large));

    bool downloadedLarge = false;
    EXPECT_CALL(*botApi, downloadFile_impl(testing::_, "large"))
        .WillOnce(testing::Invoke(
            [&downloadedLarge](const std::filesystem::path& path,
                               std::string_view) {
                downloadedLarge = true;
                std::ofstream output(path, std::ios::binary);
                output.write(reinterpret_cast<const char*>(kPng), sizeof(kPng));
                return output.good();
            }));
    expectWebPSticker();

    execute();
    EXPECT_TRUE(downloadedLarge);
    EXPECT_TRUE(stickerSent);
}

}  // namespace
