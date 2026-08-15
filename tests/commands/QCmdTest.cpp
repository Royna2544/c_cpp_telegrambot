#include <gmock/gmock.h>
#include <tgbot/types/MessageOriginUser.h>
#include <tgbot/types/PhotoSize.h>
#include <tgbot/types/User.h>
#include <tgbot/types/UserProfilePhotos.h>
#include <webp/decode.h>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

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

struct DecodedWebP {
    int width{};
    int height{};
    std::vector<std::uint8_t> rgba;
};

[[nodiscard]] std::optional<DecodedWebP> decodeWebP(const std::string& bytes) {
    int width = 0;
    int height = 0;
    if (WebPGetInfo(reinterpret_cast<const std::uint8_t*>(bytes.data()),
                    bytes.size(), &width, &height) == 0 ||
        width <= 0 || height <= 0) {
        return std::nullopt;
    }
    DecodedWebP decoded{.width = width, .height = height};
    decoded.rgba.resize(static_cast<std::size_t>(width) * height * 4U);
    if (WebPDecodeRGBAInto(reinterpret_cast<const std::uint8_t*>(bytes.data()),
                           bytes.size(), decoded.rgba.data(),
                           decoded.rgba.size(), width * 4) == nullptr) {
        return std::nullopt;
    }
    return decoded;
}

struct AvatarRegionCounts {
    std::size_t opaque{};
    std::size_t red{};
};

[[nodiscard]] AvatarRegionCounts countAvatarRegion(const DecodedWebP& image) {
    AvatarRegionCounts counts;
    // The bubble begins beyond the avatar column. Restricting the scan to the
    // leftmost 7% makes these assertions independent of bubble/text pixels.
    const int limit = std::max(1, image.width * 7 / 100);
    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < limit; ++x) {
            const auto offset =
                (static_cast<std::size_t>(y) * image.width + x) * 4U;
            const auto red = image.rgba[offset];
            const auto green = image.rgba[offset + 1];
            const auto blue = image.rgba[offset + 2];
            const auto alpha = image.rgba[offset + 3];
            if (alpha > 128)
                ++counts.opaque;
            if (alpha > 200 && red > 200 && green < 80 && blue < 80)
                ++counts.red;
        }
    }
    return counts;
}

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
        stickerData.clear();
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
            .WillOnce(testing::Invoke(
                [this](ChatId, TgBotApi::FileOrString value, auto) {
                    const auto* input = std::get_if<InputFile::Ptr>(&value);
                    if (input && *input)
                        stickerData = (*input)->data;
                    stickerSent = true;
                    return createDefaultMessage();
                }));
    }

    void useForwardedUser(UserId userId, std::string firstName) {
        auto& reply = *defaultProvidedMessage->replyToMessage;
        reply->text = "forwarded native quote";
        auto directSender = std::make_shared<TgBot::User>();
        directSender->id = userId + 1;
        directSender->firstName = "Wrong direct sender";
        reply->from = directSender;

        auto forwardedSender = std::make_shared<TgBot::User>();
        forwardedSender->id = userId;
        forwardedSender->firstName = std::move(firstName);
        auto origin = std::make_shared<TgBot::MessageOriginUser>();
        origin->senderUser = std::move(forwardedSender);
        reply->forwardOrigin = origin;
    }

    [[nodiscard]] static TgBot::UserProfilePhotos::Ptr profileWithAvatar(
        std::string fileId) {
        auto photo = std::make_shared<TgBot::PhotoSize>();
        photo->fileId = std::move(fileId);
        photo->width = 128;
        photo->height = 128;
        auto profile = std::make_shared<TgBot::UserProfilePhotos>();
        profile->totalCount = 1;
        profile->photos = {{std::move(photo)}};
        return profile;
    }

    bool stickerSent = false;
    std::string stickerData;
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

TEST_F(QCommandTest, SenderChatTakesPrecedenceOverCompatibilityUser) {
    constexpr ChatId senderChatId = -100700004;
    auto& reply = *defaultProvidedMessage->replyToMessage;
    reply->text = "anonymous administrator quote";
    reply->from = std::make_shared<TgBot::User>();
    reply->from.value()->id = 700005;
    reply->from.value()->firstName = "Anonymous compatibility user";
    reply->senderChat = std::make_shared<TgBot::Chat>();
    reply->senderChat.value()->id = senderChatId;
    reply->senderChat.value()->title = "Actual sender chat";

    EXPECT_CALL(*botApi, getUserProfilePhotos_impl(testing::_)).Times(0);
    EXPECT_CALL(*botApi, getChat_impl(senderChatId))
        .WillOnce(testing::Return(*reply->senderChat));
    expectWebPSticker();

    execute();
    EXPECT_TRUE(stickerSent);
}

TEST_F(QCommandTest, ForwardedUserUsesOriginAndRendersResolvedAvatar) {
    constexpr UserId forwardedUserId = 700004;
    useForwardedUser(forwardedUserId, "Forwarded Sender");
    EXPECT_CALL(*botApi, getUserProfilePhotos_impl(forwardedUserId))
        .WillOnce(testing::Return(profileWithAvatar("forward-avatar")));
    EXPECT_CALL(*botApi, getUserProfilePhotos_impl(forwardedUserId + 1))
        .Times(0);
    EXPECT_CALL(*botApi, downloadFile_impl(testing::_, "forward-avatar"))
        .WillOnce(testing::Invoke(
            [](const std::filesystem::path& path, std::string_view) {
                std::ofstream output(path, std::ios::binary);
                output.write(reinterpret_cast<const char*>(kPng), sizeof(kPng));
                return output.good();
            }));
    expectWebPSticker();

    execute();

    ASSERT_TRUE(stickerSent);
    const auto decoded = decodeWebP(stickerData);
    ASSERT_TRUE(decoded.has_value());
    const auto counts = countAvatarRegion(*decoded);
    EXPECT_GT(counts.opaque, 250U);
    EXPECT_GT(counts.red, 100U);
}

TEST_F(QCommandTest, ForwardedAvatarFailureUsesVisibleInitialsFallback) {
    constexpr UserId forwardedUserId = 700004;
    useForwardedUser(forwardedUserId, "Forwarded Sender");
    EXPECT_CALL(*botApi, getUserProfilePhotos_impl(forwardedUserId))
        .WillOnce(testing::Return(profileWithAvatar("missing-avatar")));
    EXPECT_CALL(*botApi, downloadFile_impl(testing::_, "missing-avatar"))
        .WillOnce(testing::Return(false));
    EXPECT_CALL(*botApi,
                sendMessage_impl(TEST_CHAT_ID,
                                 testing::HasSubstr("quote generation failed"),
                                 testing::_, testing::_, testing::_))
        .Times(0);
    expectWebPSticker();

    execute();

    ASSERT_TRUE(stickerSent);
    const auto decoded = decodeWebP(stickerData);
    ASSERT_TRUE(decoded.has_value());
    const auto counts = countAvatarRegion(*decoded);
    EXPECT_GT(counts.opaque, 250U);
    EXPECT_LT(counts.red, 20U);
}

}  // namespace
