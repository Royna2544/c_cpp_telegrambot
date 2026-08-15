#pragma once

#include <QuoteRendererExports.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected_cpp20>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace quote {

enum class QuoteOutputType : std::uint8_t { Quote, Image, Stories };
enum class QuoteOutputFormat : std::uint8_t { Png, WebP };

// quote-api accepts artwork brands that cannot all be redistributed. The
// renderer accepts those selectors for compatibility and intentionally maps
// them to the installed open emoji fallback.
enum class QuoteEmojiBrand : std::uint8_t {
    Open,
    Apple,
    Google,
    Twitter,
    Facebook,
    Samsung,
    JoyPixels,
};

enum class QuoteEntityType : std::uint8_t {
    Bold,
    Italic,
    Underline,
    Strikethrough,
    Code,
    Pre,
    TextLink,
    CustomEmoji,
};

enum class QuoteMediaType : std::uint8_t {
    Photo,
    Sticker,
    Video,
    Animation,
};

enum class QuoteErrorCode : std::uint8_t {
    InvalidRequest,
    UnsupportedFormat,
    AssetUnavailable,
    InvalidAsset,
    LimitExceeded,
    DeadlineExceeded,
    EncodeFailed,
    Internal,
};

struct QuoteError {
    QuoteErrorCode code{QuoteErrorCode::Internal};
    std::string message;
};

struct QuoteAsset {
    std::vector<std::uint8_t> bytes;
    std::string mimeType;
};

class QUOTERENDERER_EXPORT QuoteAssetResolver {
   public:
    virtual ~QuoteAssetResolver() = default;

    // assetId is an opaque Telegram file ID (or another caller-owned key).
    // Implementations may perform I/O; QuoteRenderer itself never does.
    virtual compat::expected<QuoteAsset, QuoteError> resolve(
        std::string_view assetId) = 0;
};

struct QuoteEntity {
    QuoteEntityType type{QuoteEntityType::Bold};
    // Telegram offsets and lengths are UTF-16 code units.
    std::size_t offset{};
    std::size_t length{};
    std::string url;
    std::string customEmojiId;
};

struct QuoteMediaCrop {
    // Normalized coordinates in the range [0, 1].
    double x{};
    double y{};
    double width{1.0};
    double height{1.0};
};

struct QuoteMedia {
    QuoteMediaType type{QuoteMediaType::Photo};
    std::string assetId;
    std::uint32_t width{};
    std::uint32_t height{};
    bool animated{};
    std::optional<QuoteMediaCrop> crop;
};

struct QuoteVoice {
    std::uint32_t durationSeconds{};
    std::vector<std::uint8_t> waveform;
};

struct QuoteSenderAvatar {
    std::string assetId;
};

struct QuoteSender {
    std::int64_t id{};
    std::string name;
    std::string username;
    std::optional<QuoteSenderAvatar> avatar;
};

struct QuoteReply {
    QuoteSender sender;
    std::string text;
    std::vector<QuoteEntity> entities;
    std::optional<QuoteMedia> media;
};

struct QuoteMessage {
    QuoteSender sender;
    std::string text;
    std::vector<QuoteEntity> entities;
    std::vector<QuoteMedia> media;
    std::optional<QuoteReply> reply;
    std::optional<QuoteVoice> voice;
    bool avatar{true};
    bool groupWithPrevious{};
};

struct QuoteRenderRequest {
    QuoteOutputType type{QuoteOutputType::Quote};
    QuoteOutputFormat format{QuoteOutputFormat::WebP};
    QuoteEmojiBrand emojiBrand{QuoteEmojiBrand::Open};
    std::string background{"//#292232"};
    std::uint32_t width{};
    std::uint32_t height{};
    double scale{1.0};
    std::vector<QuoteMessage> messages;

    // `/q` sets this to produce a Telegram static-sticker-compatible result:
    // WebP, no side over 512px, one side exactly 512px, and <=512 KiB.
    bool telegramSticker{};
    std::size_t maximumSourceBytes{32U * 1024U * 1024U};
    std::size_t maximumEncodedBytes{10U * 1024U * 1024U};
    std::chrono::milliseconds deadline{std::chrono::seconds(60)};
};

struct QuoteRenderResult {
    std::vector<std::uint8_t> bytes;
    std::string mimeType;
    std::string fileName;
    std::uint32_t width{};
    std::uint32_t height{};
};

class QUOTERENDERER_EXPORT QuoteRenderer {
   public:
    explicit QuoteRenderer(std::filesystem::path fontDirectory,
                           QuoteAssetResolver* resolver = nullptr);

    [[nodiscard]] compat::expected<QuoteRenderResult, QuoteError> render(
        const QuoteRenderRequest& request) const;

   private:
    std::filesystem::path fontDirectory_;
    QuoteAssetResolver* resolver_;
};

}  // namespace quote
