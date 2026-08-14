#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "CommandModulesTest.hpp"
#include "gmock/gmock.h"
#include "imagep/MediaLimits.hpp"
#include "tgbot/types/PhotoSize.h"
#include "tgbot/types/Sticker.h"

namespace {

using testing::Return;

auto hasExtension(std::string extension) {
    return testing::Truly(
        [extension = std::move(extension)](const std::filesystem::path& path) {
            return path.extension() == extension;
        });
}

void useRotateStrings(MockLocaleStrings& strings) {
    ON_CALL(strings, get(Strings::FAILED_TO_DOWNLOAD_FILE))
        .WillByDefault(Return("download failed"));
    ON_CALL(strings, get(Strings::FAILED_TO_ROTATE_IMAGE))
        .WillByDefault(Return("processing rejected"));
    ON_CALL(strings, get(Strings::CANNOT_ROTATE_NONSTATIC))
        .WillByDefault(Return("animated sticker rejected"));
    ON_CALL(strings, get(Strings::INVALID_ARGS_PASSED))
        .WillByDefault(Return("invalid arguments"));
}

}  // namespace

struct RotatePicCommandTest : CommandTestBase {
    RotatePicCommandTest() : CommandTestBase("rotatepic") {}

    TgBot::PhotoSize::Ptr replyWithPhoto() {
        defaultProvidedMessage->replyToMessage = createDefaultMessage();
        auto photo = std::make_shared<TgBot::PhotoSize>();
        photo->fileId = TEST_MEDIA_ID;
        photo->width = 1920;
        photo->height = 1080;
        (*defaultProvidedMessage->replyToMessage)->photo = {{photo}};
        return photo;
    }

    TgBot::Sticker::Ptr replyWithSticker() {
        defaultProvidedMessage->replyToMessage = createDefaultMessage();
        auto sticker = std::make_shared<TgBot::Sticker>();
        sticker->fileId = TEST_MEDIA_ID;
        sticker->width = 512;
        sticker->height = 512;
        sticker->isAnimated = false;
        sticker->isVideo = false;
        (*defaultProvidedMessage->replyToMessage)->sticker = sticker;
        return sticker;
    }
};

TEST_F(RotatePicCommandTest, DownloadsTelegramPhotoAsJpeg) {
    useRotateStrings(strings);
    replyWithPhoto();
    setCommandExtArgs({"90"});

    std::filesystem::path privateDirectory;
    EXPECT_CALL(*random,
                generate(0, std::numeric_limits<RandomBase::ret_type>::max()))
        .WillOnce(Return(12345))
        .WillOnce(Return(54321));
    EXPECT_CALL(*botApi, downloadFile_impl(hasExtension(".jpg"), TEST_MEDIA_ID))
        .WillOnce(testing::Invoke(
            [&](const std::filesystem::path& path, std::string_view) {
                privateDirectory = path.parent_path();
                EXPECT_TRUE(std::filesystem::is_directory(privateDirectory));
                EXPECT_THAT(privateDirectory.filename().string(),
                            testing::StartsWith("glider_rotatepic_"));
#ifndef _WIN32
                std::error_code ec;
                const auto permissions =
                    std::filesystem::status(privateDirectory, ec).permissions();
                EXPECT_FALSE(ec);
                EXPECT_EQ(permissions & (std::filesystem::perms::group_all |
                                         std::filesystem::perms::others_all),
                          std::filesystem::perms::none);
#endif
                return false;
            }));
    EXPECT_CALL(*botApi,
                submitCommandWork("rotatepic", TgBotApi::WorkClass::Media,
                                  testing::_, testing::_));
    willSendReplyMessage("download failed");

    execute();
    EXPECT_FALSE(privateDirectory.empty());
    EXPECT_FALSE(std::filesystem::exists(privateDirectory));
}

TEST_F(RotatePicCommandTest, DownloadsStaticStickerAsWebp) {
    useRotateStrings(strings);
    replyWithSticker();
    setCommandExtArgs({"90"});

    EXPECT_CALL(*random,
                generate(0, std::numeric_limits<RandomBase::ret_type>::max()))
        .WillOnce(Return(67890))
        .WillOnce(Return(9876));
    EXPECT_CALL(*botApi,
                downloadFile_impl(hasExtension(".webp"), TEST_MEDIA_ID))
        .WillOnce(Return(false));
    willSendReplyMessage("download failed");

    execute();
}

TEST_F(RotatePicCommandTest, RejectsAnimatedTgsStickerBeforeDownload) {
    useRotateStrings(strings);
    auto sticker = replyWithSticker();
    sticker->isAnimated = true;
    setCommandExtArgs({"90"});

    EXPECT_CALL(*botApi, downloadFile_impl(testing::_, testing::_)).Times(0);
    willSendReplyMessage("animated sticker rejected");

    execute();
}

TEST_F(RotatePicCommandTest, RejectsOversizedMetadataBeforeDownload) {
    useRotateStrings(strings);
    auto photo = replyWithPhoto();
    photo->fileSize =
        static_cast<std::int32_t>(imagep::limits::kMaxCompressedInputBytes + 1);
    setCommandExtArgs({"90"});

    EXPECT_CALL(*botApi, downloadFile_impl(testing::_, testing::_)).Times(0);
    willSendReplyMessage("processing rejected");

    execute();
}

TEST_F(RotatePicCommandTest, RejectsUnknownTransformBeforeDownload) {
    useRotateStrings(strings);
    replyWithPhoto();
    setCommandExtArgs({"90", "sepia"});

    EXPECT_CALL(*botApi, downloadFile_impl(testing::_, testing::_)).Times(0);
    willSendReplyMessage("invalid arguments");

    execute();
}

TEST_F(RotatePicCommandTest, CancellationStopsBeforeDownload) {
    useRotateStrings(strings);
    replyWithPhoto();
    setCommandExtArgs({"90"});

    EXPECT_CALL(*botApi, downloadFile_impl(testing::_, testing::_)).Times(0);
    EXPECT_CALL(*botApi,
                submitCommandWork("rotatepic", TgBotApi::WorkClass::Media,
                                  testing::_, testing::_))
        .WillOnce(testing::Invoke([](std::string_view, TgBotApi::WorkClass,
                                     TgBotApi::CancellableWork work,
                                     TgBotApi::WorkOptions) {
            std::stop_source source;
            source.request_stop();
            work(source.get_token());
            return std::optional<TgBotApi::WorkId>{1};
        }));

    execute();
}
