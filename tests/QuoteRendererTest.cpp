#include <gtest/gtest.h>
#include <png.h>
#include <webp/decode.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstdint>
#include <functional>
#include <future>
#include <optional>
#include <quote/QuoteRenderer.hpp>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

[[nodiscard]] std::filesystem::path testFontDirectory() {
    return QUOTE_TEST_FONT_DIR;
}

// A valid one-pixel PNG. Real media dimensions/crop behavior is covered by the
// renderer's decoded-size validation; the command tests exercise Telegram file
// extraction separately.
constexpr std::uint8_t kPng[] = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
    0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
    0x08, 0x06, 0x00, 0x00, 0x00, 0x1f, 0x15, 0xc4, 0x89, 0x00, 0x00, 0x00,
    0x0d, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x63, 0xf8, 0xcf, 0xc0, 0xf0,
    0x1f, 0x00, 0x05, 0x00, 0x01, 0xff, 0x89, 0x99, 0x3d, 0x1d, 0x00, 0x00,
    0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82,
};

class TestResolver final : public quote::QuoteAssetResolver {
   public:
    compat::expected<quote::QuoteAsset, quote::QuoteError> resolve(
        std::string_view assetId) override {
        ++calls;
        if (assetId == "missing") {
            return compat::unexpected<quote::QuoteError>(quote::QuoteError{
                .code = quote::QuoteErrorCode::AssetUnavailable,
                .message = "missing",
            });
        }
        return quote::QuoteAsset{
            .bytes =
                std::vector<std::uint8_t>(std::begin(kPng), std::end(kPng)),
            .mimeType = "image/png",
        };
    }

    std::atomic<int> calls{};
};

quote::QuoteRenderRequest baseRequest() {
    quote::QuoteRenderRequest request;
    request.messages.push_back(quote::QuoteMessage{
        .sender = {.id = 1, .name = "Ada", .username = "ada"},
        .text = "Native quote rendering",
    });
    return request;
}

bool startsWith(const std::vector<std::uint8_t>& value,
                std::initializer_list<std::uint8_t> prefix) {
    return value.size() >= prefix.size() &&
           std::equal(prefix.begin(), prefix.end(), value.begin());
}

struct DecodedPng {
    std::uint32_t width{};
    std::uint32_t height{};
    std::vector<std::uint8_t> rgba;
};

std::optional<DecodedPng> decodePng(const std::vector<std::uint8_t>& bytes) {
    png_image image{};
    image.version = PNG_IMAGE_VERSION;
    if (png_image_begin_read_from_memory(&image, bytes.data(), bytes.size()) ==
        0) {
        return std::nullopt;
    }
    image.format = PNG_FORMAT_RGBA;
    DecodedPng decoded{.width = image.width, .height = image.height};
    decoded.rgba.resize(PNG_IMAGE_SIZE(image));
    const bool success =
        png_image_finish_read(&image, nullptr, decoded.rgba.data(), 0,
                              nullptr) != 0;
    png_image_free(&image);
    if (!success)
        return std::nullopt;
    return decoded;
}

std::optional<DecodedPng> decodeWebP(const std::vector<std::uint8_t>& bytes) {
    WebPBitstreamFeatures features{};
    if (WebPGetFeatures(bytes.data(), bytes.size(), &features) !=
            VP8_STATUS_OK ||
        features.width <= 0 || features.height <= 0) {
        return std::nullopt;
    }
    DecodedPng decoded{
        .width = static_cast<std::uint32_t>(features.width),
        .height = static_cast<std::uint32_t>(features.height),
    };
    decoded.rgba.resize(static_cast<std::size_t>(decoded.width) *
                        decoded.height * 4U);
    if (WebPDecodeRGBAInto(bytes.data(), bytes.size(), decoded.rgba.data(),
                           decoded.rgba.size(),
                           static_cast<int>(decoded.width * 4U)) == nullptr) {
        return std::nullopt;
    }
    return decoded;
}

// A compact perceptual golden for the sender/body area. Each bit describes
// whether a cell contains enough high-luminance foreground pixels. This is
// insensitive to small platform antialiasing differences while detecting font
// fallback, shaping, or layout regressions across Latin, Hangul, RTL, and
// emoji.
std::array<std::uint64_t, 4> foregroundHash(const DecodedPng& image) {
    constexpr std::uint32_t columns = 32;
    constexpr std::uint32_t rows = 8;
    constexpr std::uint32_t left = 26;
    constexpr std::uint32_t top = 78;
    constexpr std::uint32_t width = 460;
    constexpr std::uint32_t height = 58;
    std::array<std::uint64_t, 4> result{};
    if (image.width < left + width || image.height < top + height)
        return result;

    for (std::uint32_t row = 0; row < rows; ++row) {
        const auto y0 = top + row * height / rows;
        const auto y1 = top + (row + 1) * height / rows;
        for (std::uint32_t column = 0; column < columns; ++column) {
            const auto x0 = left + column * width / columns;
            const auto x1 = left + (column + 1) * width / columns;
            std::uint32_t foreground = 0;
            std::uint32_t samples = 0;
            for (auto y = y0; y < y1; ++y) {
                for (auto x = x0; x < x1; ++x) {
                    const auto offset =
                        (static_cast<std::size_t>(y) * image.width + x) * 4U;
                    const auto red = image.rgba[offset];
                    const auto green = image.rgba[offset + 1];
                    const auto blue = image.rgba[offset + 2];
                    const auto luminance =
                        (299U * red + 587U * green + 114U * blue) / 1000U;
                    foreground += luminance >= 115U ? 1U : 0U;
                    ++samples;
                }
            }
            const auto bit = row * columns + column;
            if (foreground * 16U >= samples) {
                result[bit / 64U] |= std::uint64_t{1} << (bit % 64U);
            }
        }
    }
    return result;
}

std::uint32_t hammingDistance(const std::array<std::uint64_t, 4>& left,
                              const std::array<std::uint64_t, 4>& right) {
    std::uint32_t result = 0;
    for (std::size_t i = 0; i < left.size(); ++i)
        result += std::popcount(left[i] ^ right[i]);
    return result;
}

std::uint64_t countPixels(
    const DecodedPng& image, std::uint32_t left, std::uint32_t top,
    std::uint32_t width, std::uint32_t height,
    const std::function<bool(std::uint8_t, std::uint8_t, std::uint8_t,
                             std::uint8_t)>& predicate) {
    const auto right = std::min(image.width, left + width);
    const auto bottom = std::min(image.height, top + height);
    std::uint64_t count = 0;
    for (auto y = top; y < bottom; ++y) {
        for (auto x = left; x < right; ++x) {
            const auto offset =
                (static_cast<std::size_t>(y) * image.width + x) * 4U;
            count += predicate(image.rgba[offset], image.rgba[offset + 1],
                               image.rgba[offset + 2], image.rgba[offset + 3])
                         ? 1U
                         : 0U;
        }
    }
    return count;
}

TEST(QuoteRenderer, RejectsMissingExplicitFontDirectory) {
    quote::QuoteRenderer renderer(testFontDirectory() / "missing");
    auto result = renderer.render(baseRequest());

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, quote::QuoteErrorCode::AssetUnavailable);
}

TEST(QuoteRenderer, ProducesTelegramCompliantWebPSticker) {
    auto request = baseRequest();
    request.telegramSticker = true;
    request.scale = 2.0;
    request.background = "//#292232";
    request.maximumEncodedBytes = 512U * 1024U;

    quote::QuoteRenderer renderer(testFontDirectory());
    auto result = renderer.render(request);

    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_EQ(result.value().mimeType, "image/webp");
    EXPECT_EQ(result.value().fileName, "quote.webp");
    EXPECT_LE(result.value().bytes.size(), 512U * 1024U);
    EXPECT_LE(result.value().width, 512U);
    EXPECT_LE(result.value().height, 512U);
    EXPECT_TRUE(result.value().width == 512U || result.value().height == 512U);
    EXPECT_EQ(result.value().width, 512U);
    EXPECT_GE(result.value().height, 120U);
    EXPECT_LE(result.value().height, 320U);
    EXPECT_TRUE(startsWith(result.value().bytes, {'R', 'I', 'F', 'F'}));
    ASSERT_GE(result.value().bytes.size(), 12U);
    EXPECT_EQ(std::string(result.value().bytes.begin() + 8,
                          result.value().bytes.begin() + 12),
              "WEBP");

    const auto decoded = decodeWebP(result.value().bytes);
    ASSERT_TRUE(decoded.has_value());
    ASSERT_EQ(decoded->width, result.value().width);
    ASSERT_EQ(decoded->height, result.value().height);
    EXPECT_GE(decoded->height, 180U)
        << "the old unreadable strip was only 128px high";

    std::uint32_t opaqueTop = decoded->height;
    std::uint32_t opaqueBottom = 0;
    for (std::uint32_t y = 0; y < decoded->height; ++y) {
        for (std::uint32_t x = 0; x < decoded->width; ++x) {
            const auto offset =
                (static_cast<std::size_t>(y) * decoded->width + x) * 4U;
            if (decoded->rgba[offset + 3] < 180U)
                continue;
            opaqueTop = std::min(opaqueTop, y);
            opaqueBottom = std::max(opaqueBottom, y);
        }
    }
    ASSERT_LT(opaqueTop, decoded->height);
    EXPECT_GE(opaqueBottom - opaqueTop + 1U, 100U)
        << "the normalized card must remain tall enough for readable text";

    const auto avatarInk =
        countPixels(*decoded, 0, 0, 110, decoded->height,
                    [](auto, auto, auto, auto alpha) { return alpha >= 180U; });
    const auto readableInk =
        countPixels(*decoded, 110, 0, decoded->width - 110, decoded->height,
                    [](auto red, auto green, auto blue, auto alpha) {
                        return alpha >= 180U && red >= 180U && green >= 180U &&
                               blue >= 180U;
                    });
    EXPECT_GT(avatarInk, 2'500U)
        << "the sender avatar must not collapse to a small dot";
    EXPECT_GT(readableInk, 200U)
        << "sender and message text must survive final sticker scaling";
}

TEST(QuoteRenderer, ShrinkWrapsRoundedQuoteWithReadableTextAndInitialsAvatar) {
    TestResolver resolver;
    quote::QuoteRenderer renderer(testFontDirectory(), &resolver);
    auto request = baseRequest();
    request.type = quote::QuoteOutputType::Quote;
    request.format = quote::QuoteOutputFormat::Png;
    request.width = 512;
    request.scale = 2.0;
    request.background = "//#292232";
    request.messages.front().sender.avatar =
        quote::QuoteSenderAvatar{.assetId = "missing"};

    auto result = renderer.render(request);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_GT(result.value().width, 300U);
    EXPECT_LT(result.value().width, 1024U);
    EXPECT_GT(result.value().height, 120U);
    EXPECT_LT(result.value().height, 400U);
    EXPECT_GT(result.value().width, result.value().height);

    const auto decoded = decodePng(result.value().bytes);
    ASSERT_TRUE(decoded.has_value());
    ASSERT_EQ(decoded->width, result.value().width);
    ASSERT_EQ(decoded->height, result.value().height);
    const auto corner = decoded->rgba[3];
    EXPECT_EQ(corner, 0U) << "quote exterior must remain transparent";

    const auto avatarInk =
        countPixels(*decoded, 0, 0, 100, decoded->height,
                    [](auto, auto, auto, auto alpha) { return alpha >= 180U; });
    const auto avatarWhite =
        countPixels(*decoded, 0, 0, 100, decoded->height,
                    [](auto red, auto green, auto blue, auto alpha) {
                        return alpha >= 180U && red >= 220U && green >= 220U &&
                               blue >= 220U;
                    });
    EXPECT_GT(avatarInk, 4'000U)
        << "avatar fallback should be a filled, readable circle";
    EXPECT_GT(avatarWhite, 20U)
        << "avatar fallback should render sender initials";

    const auto purpleCard =
        countPixels(*decoded, 110, 0, decoded->width - 110, decoded->height,
                    [](auto red, auto green, auto blue, auto alpha) {
                        return alpha >= 180U && red > green && blue > green;
                    });
    const auto readableInk =
        countPixels(*decoded, 120, 0, decoded->width - 120, decoded->height,
                    [](auto red, auto green, auto blue, auto alpha) {
                        return alpha >= 180U && red >= 180U && green >= 180U &&
                               blue >= 180U;
                    });
    EXPECT_GT(purpleCard, 8'000U)
        << "the bubble should use the purple gradient, not a dark strip";
    EXPECT_GT(readableInk, 200U)
        << "sender and message text should occupy a readable pixel area";
}

TEST(QuoteRenderer, NaturalQuoteWidthTracksTextInsteadOfFillingCanvas) {
    quote::QuoteRenderer renderer(testFontDirectory());
    auto shortRequest = baseRequest();
    shortRequest.format = quote::QuoteOutputFormat::Png;
    shortRequest.width = 512;
    shortRequest.scale = 2.0;
    shortRequest.messages.front().text = "Hi";
    auto longRequest = shortRequest;
    longRequest.messages.front().text =
        "A substantially longer quote that should create a wider card";

    auto shortResult = renderer.render(shortRequest);
    auto longResult = renderer.render(longRequest);
    ASSERT_TRUE(shortResult.has_value()) << shortResult.error().message;
    ASSERT_TRUE(longResult.has_value()) << longResult.error().message;
    EXPECT_LT(shortResult.value().width, longResult.value().width);
    EXPECT_LT(longResult.value().width, 1024U);
}

TEST(QuoteRenderer, RendersInternationalTextEntitiesReplyMediaVoiceAndEmoji) {
    TestResolver resolver;
    quote::QuoteRenderer renderer(testFontDirectory(), &resolver);
    quote::QuoteRenderRequest request;
    request.type = quote::QuoteOutputType::Image;
    request.format = quote::QuoteOutputFormat::Png;
    request.width = 720;
    request.height = 900;
    request.background = "#15202b";
    request.emojiBrand = quote::QuoteEmojiBrand::Apple;
    request.messages.push_back(quote::QuoteMessage{
        .sender = {.id = 7,
                   .name = "한글 사용자",
                   .username = "korean",
                   .avatar = quote::QuoteSenderAvatar{.assetId = "avatar"}},
        .text = "한국어 😀 العربية RTL and bold",
        .entities = {{.type = quote::QuoteEntityType::Bold,
                      .offset = 23,
                      .length = 4},
                     {.type = quote::QuoteEntityType::CustomEmoji,
                      .offset = 4,
                      .length = 2,
                      .customEmojiId = "emoji"}},
        .media = {{.type = quote::QuoteMediaType::Photo,
                   .assetId = "photo",
                   .width = 640,
                   .height = 480,
                   .crop =
                       quote::QuoteMediaCrop{
                           .x = 0.1, .y = 0.1, .width = 0.8, .height = 0.8}}},
        .reply =
            quote::QuoteReply{
                .sender = {.id = 8, .name = "Reply Sender"},
                .text = "이전 메시지 / previous message",
                .entities = {{.type = quote::QuoteEntityType::Italic,
                              .offset = 0,
                              .length = 6}},
                .media =
                    quote::QuoteMedia{
                        .type = quote::QuoteMediaType::Photo,
                        .assetId = "reply-photo",
                    },
            },
        .voice =
            quote::QuoteVoice{
                .durationSeconds = 73,
                .waveform = {0, 32, 64, 128, 255, 128, 64, 32},
            },
    });

    auto result = renderer.render(request);

    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_EQ(result.value().mimeType, "image/png");
    EXPECT_EQ(result.value().width, 720U);
    EXPECT_EQ(result.value().height, 900U);
    EXPECT_TRUE(startsWith(result.value().bytes,
                           {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a}));
    EXPECT_GE(resolver.calls.load(), 4);
}

TEST(QuoteRenderer, SupportsQuoteImageAndStoriesDefaults) {
    quote::QuoteRenderer renderer(testFontDirectory());
    for (const auto type :
         {quote::QuoteOutputType::Quote, quote::QuoteOutputType::Image,
          quote::QuoteOutputType::Stories}) {
        auto request = baseRequest();
        request.type = type;
        request.format = quote::QuoteOutputFormat::Png;
        auto result = renderer.render(request);
        ASSERT_TRUE(result.has_value()) << result.error().message;
        EXPECT_FALSE(result.value().bytes.empty());
        if (type == quote::QuoteOutputType::Image) {
            EXPECT_EQ(result.value().width, 1200U);
            EXPECT_EQ(result.value().height, 630U);
        } else if (type == quote::QuoteOutputType::Stories) {
            EXPECT_EQ(result.value().width, 1080U);
            EXPECT_EQ(result.value().height, 1920U);
        }
    }
}

TEST(QuoteRenderer, LongQuoteIsScaledIntoStickerBounds) {
    auto request = baseRequest();
    request.telegramSticker = true;
    request.messages.front().text =
        std::string(3000, 'a') + " 한국어 😀 العربية";

    quote::QuoteRenderer renderer(testFontDirectory());
    auto result = renderer.render(request);

    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_LE(result.value().width, 512U);
    EXPECT_LE(result.value().height, 512U);
    EXPECT_TRUE(result.value().width == 512U || result.value().height == 512U);
}

TEST(QuoteRenderer, RejectsInvalidCropAndOversizedSourceBudget) {
    TestResolver resolver;
    quote::QuoteRenderer renderer(testFontDirectory(), &resolver);
    auto request = baseRequest();
    request.messages.front().media.push_back(quote::QuoteMedia{
        .type = quote::QuoteMediaType::Photo,
        .assetId = "photo",
        .crop =
            quote::QuoteMediaCrop{.x = 0.8, .y = 0, .width = 0.5, .height = 1},
    });
    auto invalidCrop = renderer.render(request);
    ASSERT_FALSE(invalidCrop.has_value());
    EXPECT_EQ(invalidCrop.error().code, quote::QuoteErrorCode::InvalidRequest);

    request.messages.front().media.front().crop.reset();
    request.maximumSourceBytes = 8;
    auto oversized = renderer.render(request);
    ASSERT_FALSE(oversized.has_value());
    EXPECT_EQ(oversized.error().code, quote::QuoteErrorCode::LimitExceeded);
}

TEST(QuoteRenderer, ConcurrentRendersDoNotShareMutableState) {
    TestResolver resolver;
    quote::QuoteRenderer firstRenderer(testFontDirectory(), &resolver);
    quote::QuoteRenderer secondRenderer(testFontDirectory(), &resolver);
    std::vector<std::future<bool>> futures;
    for (int i = 0; i < 8; ++i) {
        futures.push_back(std::async(
            std::launch::async, [&firstRenderer, &secondRenderer, i] {
                auto request = baseRequest();
                request.messages.front().text += " #" + std::to_string(i);
                request.messages.front().media.push_back(quote::QuoteMedia{
                    .type = quote::QuoteMediaType::Sticker,
                    .assetId = "asset",
                });
                auto& renderer = (i % 2 == 0) ? firstRenderer : secondRenderer;
                auto result = renderer.render(request);
                return result.has_value() && !result.value().bytes.empty();
            }));
    }
    for (auto& future : futures)
        EXPECT_TRUE(future.get());
}

TEST(QuoteRenderer, TextBackendContentionHonorsRenderDeadline) {
    struct RenderOutcome {
        bool hasValue{};
        quote::QuoteErrorCode errorCode{quote::QuoteErrorCode::Internal};
        std::string errorMessage;
    };
    const auto renderOutcome = [](quote::QuoteRenderer& renderer,
                                  const quote::QuoteRenderRequest& request) {
        auto result = renderer.render(request);
        if (result.has_value())
            return RenderOutcome{.hasValue = true};
        return RenderOutcome{.hasValue = false,
                             .errorCode = result.error().code,
                             .errorMessage = result.error().message};
    };

    class BlockingResolver final : public quote::QuoteAssetResolver {
       public:
        explicit BlockingResolver(std::shared_future<void> release)
            : release_(std::move(release)) {}

        std::future<void> takeEnteredFuture() { return entered_.get_future(); }

        compat::expected<quote::QuoteAsset, quote::QuoteError> resolve(
            std::string_view) override {
            entered_.set_value();
            release_.wait();
            return quote::QuoteAsset{
                .bytes =
                    std::vector<std::uint8_t>(std::begin(kPng), std::end(kPng)),
                .mimeType = "image/png",
            };
        }

       private:
        std::promise<void> entered_;
        std::shared_future<void> release_;
    };

    std::promise<void> releasePromise;
    BlockingResolver blockingResolver(releasePromise.get_future().share());
    auto resolverEntered = blockingResolver.takeEnteredFuture();
    quote::QuoteRenderer blockingRenderer(testFontDirectory(),
                                          &blockingResolver);
    auto blockingRequest = baseRequest();
    blockingRequest.messages.front().media.push_back(quote::QuoteMedia{
        .type = quote::QuoteMediaType::Photo,
        .assetId = "blocking",
    });
    auto blockingRender =
        std::async(std::launch::async, [&blockingRenderer, &renderOutcome,
                                        request = std::move(blockingRequest)] {
            return renderOutcome(blockingRenderer, request);
        });

    if (resolverEntered.wait_for(std::chrono::seconds(5)) !=
        std::future_status::ready) {
        releasePromise.set_value();
        (void)blockingRender.get();
        FAIL() << "blocking render never acquired the text backend";
    }

    quote::QuoteRenderer waitingRenderer(testFontDirectory());
    auto waitingRequest = baseRequest();
    waitingRequest.deadline = std::chrono::milliseconds(30);
    std::promise<void> waitingStartedPromise;
    auto waitingStarted = waitingStartedPromise.get_future();
    auto waitingRender =
        std::async(std::launch::async,
                   [&waitingRenderer, &waitingStartedPromise, &renderOutcome,
                    request = std::move(waitingRequest)] {
                       waitingStartedPromise.set_value();
                       return renderOutcome(waitingRenderer, request);
                   });

    if (waitingStarted.wait_for(std::chrono::seconds(5)) !=
        std::future_status::ready) {
        releasePromise.set_value();
        (void)blockingRender.get();
        (void)waitingRender.get();
        FAIL() << "waiting render thread did not start";
    }
    const auto waitingStatus = waitingRender.wait_for(std::chrono::seconds(5));
    EXPECT_EQ(waitingStatus, std::future_status::ready)
        << "text-backend contention ignored the render deadline";
    const bool releaseWasNeeded = waitingStatus != std::future_status::ready;
    if (releaseWasNeeded)
        releasePromise.set_value();

    auto waitingResult = waitingRender.get();
    if (!releaseWasNeeded)
        releasePromise.set_value();
    auto blockingResult = blockingRender.get();

    ASSERT_FALSE(waitingResult.hasValue);
    EXPECT_EQ(waitingResult.errorCode, quote::QuoteErrorCode::DeadlineExceeded);
    ASSERT_TRUE(blockingResult.hasValue) << blockingResult.errorMessage;
}

TEST(QuoteRenderer, RejectsMalformedUtf8EntityRangesAndTinyCanvas) {
    quote::QuoteRenderer renderer(testFontDirectory());
    auto request = baseRequest();
    request.messages.front().text = std::string("bad\xff", 4);
    auto invalidUtf8 = renderer.render(request);
    ASSERT_FALSE(invalidUtf8.has_value());
    EXPECT_EQ(invalidUtf8.error().code, quote::QuoteErrorCode::InvalidRequest);

    request = baseRequest();
    request.messages.front().entities.push_back(quote::QuoteEntity{
        .type = quote::QuoteEntityType::Bold,
        .offset = 100,
        .length = 1,
    });
    auto invalidEntity = renderer.render(request);
    ASSERT_FALSE(invalidEntity.has_value());
    EXPECT_EQ(invalidEntity.error().code,
              quote::QuoteErrorCode::InvalidRequest);

    request = baseRequest();
    request.width = 32;
    auto tinyCanvas = renderer.render(request);
    ASSERT_FALSE(tinyCanvas.has_value());
    EXPECT_EQ(tinyCanvas.error().code, quote::QuoteErrorCode::LimitExceeded);
}

TEST(QuoteRenderer, EnforcesEncodeLimitAndDeadlineAndFallsBackForAvatar) {
    TestResolver resolver;
    quote::QuoteRenderer renderer(testFontDirectory(), &resolver);
    auto request = baseRequest();
    request.format = quote::QuoteOutputFormat::Png;
    request.maximumEncodedBytes = 1;
    auto bounded = renderer.render(request);
    ASSERT_FALSE(bounded.has_value());
    EXPECT_EQ(bounded.error().code, quote::QuoteErrorCode::LimitExceeded);

    request = baseRequest();
    request.messages.front().sender.avatar =
        quote::QuoteSenderAvatar{.assetId = "missing"};
    auto fallback = renderer.render(request);
    ASSERT_TRUE(fallback.has_value()) << fallback.error().message;

    class SlowResolver final : public quote::QuoteAssetResolver {
       public:
        compat::expected<quote::QuoteAsset, quote::QuoteError> resolve(
            std::string_view) override {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            return quote::QuoteAsset{
                .bytes =
                    std::vector<std::uint8_t>(std::begin(kPng), std::end(kPng)),
                .mimeType = "image/png",
            };
        }
    } slowResolver;
    quote::QuoteRenderer slowRenderer(testFontDirectory(), &slowResolver);
    request = baseRequest();
    request.deadline = std::chrono::milliseconds(1);
    request.messages.front().media.push_back(quote::QuoteMedia{
        .type = quote::QuoteMediaType::Photo,
        .assetId = "slow",
    });
    auto expired = slowRenderer.render(request);
    ASSERT_FALSE(expired.has_value());
    EXPECT_EQ(expired.error().code, quote::QuoteErrorCode::DeadlineExceeded);
}

TEST(QuoteRenderer, MatchesBundledFontPerceptualGoldenAndMapsEmojiBrands) {
    quote::QuoteRenderer renderer(testFontDirectory());
    auto request = baseRequest();
    request.type = quote::QuoteOutputType::Image;
    request.format = quote::QuoteOutputFormat::Png;
    request.width = 512;
    request.height = 240;
    request.background = "#203040";
    request.messages.front().avatar = false;
    request.messages.front().sender.name = "Noto 검증 العربية";
    request.messages.front().text = "Latin · 한글 · العربية · 😀";

    auto rendered = renderer.render(request);
    ASSERT_TRUE(rendered.has_value()) << rendered.error().message;
    const auto decoded = decodePng(rendered.value().bytes);
    ASSERT_TRUE(decoded.has_value());
    const auto actual = foregroundHash(*decoded);
    // Captured from this exact pinned-font request. The tolerance permits
    // small rasterizer differences while still detecting broken CJK/RTL/emoji
    // shaping or a materially different sender/body layout.
    constexpr std::array<std::uint64_t, 4> expected = {
        0x0001ff8000013f80ULL, 0x000000000000d800ULL, 0x0080400000000000ULL,
        0x00ffff8000d2f780ULL};
    EXPECT_LE(hammingDistance(actual, expected), 12U)
        << std::hex << actual[0] << ' ' << actual[1] << ' ' << actual[2] << ' '
        << actual[3];

    const auto openBytes = rendered.value().bytes;
    for (const auto brand :
         {quote::QuoteEmojiBrand::Apple, quote::QuoteEmojiBrand::Google,
          quote::QuoteEmojiBrand::Twitter, quote::QuoteEmojiBrand::Facebook,
          quote::QuoteEmojiBrand::Samsung, quote::QuoteEmojiBrand::JoyPixels}) {
        request.emojiBrand = brand;
        auto fallback = renderer.render(request);
        ASSERT_TRUE(fallback.has_value()) << fallback.error().message;
        EXPECT_EQ(fallback.value().bytes, openBytes);
    }
}

}  // namespace
