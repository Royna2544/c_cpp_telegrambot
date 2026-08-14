#include <absl/log/log.h>
#include <cairo.h>
#include <pango/pangocairo.h>
#include <png.h>
#include <turbojpeg.h>
#include <webp/decode.h>
#include <webp/encode.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <filesystem>
#include <limits>
#include <memory>
#ifdef QUOTE_HAVE_OPENCV
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#endif
#include <optional>
#include <quote/QuoteRenderer.hpp>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace quote {
namespace {

constexpr std::uint32_t kMaximumDimension = 4096;
constexpr std::uint64_t kMaximumPixels = 64ULL * 1024ULL * 1024ULL;
constexpr std::size_t kMaximumMessages = 20;
constexpr std::size_t kMaximumMediaPerMessage = 10;
constexpr std::size_t kMaximumTextBytes = 32U * 1024U;
constexpr std::size_t kMaximumMetadataBytes = 32U * 1024U;
constexpr std::size_t kMaximumEntities = 512;
constexpr std::size_t kMaximumWaveformBytes = 1024;
constexpr std::size_t kMaximumAssetIdBytes = 4096;
constexpr std::size_t kMaximumBackgroundBytes = 256;
constexpr std::size_t kTelegramStickerBytes = 512U * 1024U;
constexpr auto kMaximumDeadline = std::chrono::minutes(5);
constexpr double kPi = 3.14159265358979323846;
constexpr std::array<std::string_view, 4> kBundledFontFiles = {
    "NotoSans-Variable.ttf", "NotoSansKR-Variable.ttf",
    "NotoSansArabic-Variable.ttf", "NotoEmoji-Variable.ttf"};

using SurfacePtr =
    std::unique_ptr<cairo_surface_t, decltype(&cairo_surface_destroy)>;
using CairoPtr = std::unique_ptr<cairo_t, decltype(&cairo_destroy)>;
using LayoutPtr = std::unique_ptr<PangoLayout, decltype(&g_object_unref)>;
using FontPtr = std::unique_ptr<PangoFontDescription,
                                decltype(&pango_font_description_free)>;
using AttrListPtr =
    std::unique_ptr<PangoAttrList, decltype(&pango_attr_list_unref)>;

struct Rgba {
    double red{};
    double green{};
    double blue{};
    double alpha{1.0};
};

struct DecodedImage {
    int width{};
    int height{};
    int stride{};
    // Native-endian Cairo ARGB32 (BGRA bytes on little-endian hosts).
    std::vector<std::uint8_t> cairoArgb;
};

struct RenderState {
    const QuoteRenderRequest& request;
    QuoteAssetResolver* resolver{};
    std::chrono::steady_clock::time_point started;
    std::chrono::steady_clock::time_point expiresAt;
    std::size_t sourceBytes{};

    [[nodiscard]] bool expired() const {
        return std::chrono::steady_clock::now() >= expiresAt;
    }
};

struct MessageMetrics {
    double height{};
    double textHeight{};
    double replyHeight{};
    double replyMediaSize{};
    std::vector<double> mediaHeights;
};

[[nodiscard]] bool containsBundledFonts(
    const std::filesystem::path& directory) {
    std::error_code error;
    if (!std::filesystem::is_directory(directory, error) || error)
        return false;
    for (const auto file : kBundledFontFiles) {
        if (!std::filesystem::is_regular_file(directory / file, error) ||
            error) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::string pangoFontPath(const std::filesystem::path& path) {
#if defined(_WIN32)
    const auto encoded = path.u8string();
    return {reinterpret_cast<const char*>(encoded.data()), encoded.size()};
#else
    return path.string();
#endif
}

[[nodiscard]] bool pinDynamicPangoRuntime() noexcept {
#ifdef _WIN32
    // GObject keeps registered Pango type metadata for the life of the
    // process. If /q is the last consumer, unloading Pango and loading it again
    // attempts to register those names twice. Taking the address of an imported
    // function is not sufficient here: some toolchains return the command
    // module's IAT thunk rather than the address inside the Pango DLL. Resolve
    // the export from the loaded DLL, then pin the module containing that exact
    // address.
    constexpr std::array moduleNames = {L"pangocairo-1.0-0.dll",
                                        L"libpangocairo-1.0-0.dll"};
    for (const auto* moduleName : moduleNames) {
        const HMODULE module = GetModuleHandleW(moduleName);
        if (module == nullptr)
            continue;
        const auto function =
            GetProcAddress(module, "pango_cairo_font_map_get_default");
        if (function == nullptr)
            continue;
        HMODULE pinnedModule = nullptr;
        return GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                      GET_MODULE_HANDLE_EX_FLAG_PIN,
                                  reinterpret_cast<LPCWSTR>(function),
                                  &pinnedModule) != FALSE;
    }
    return false;
#else
    return true;
#endif
}

[[nodiscard]] std::optional<QuoteError> registerBundledFonts(
    const std::filesystem::path& directory) {
    // Pango's default Cairo font map is per-thread. Force the portable
    // FreeType/fontconfig backend so the same bundled outline fonts are used
    // on Windows, macOS, Linux, and ARM, then mark that thread-owned map after
    // registration. The marker lives in Pango rather than this unloadable
    // module, avoiding unsafe thread_local destructors across module reloads.
    constexpr char registrationKey[] =
        "glider-quote-fonts-352f6b7d9d6cc4fa9e242b931291d31b21a6dc84";
    if (!containsBundledFonts(directory)) {
        return QuoteError{.code = QuoteErrorCode::AssetUnavailable,
                          .message = "bundled quote fonts are unavailable"};
    }
    if (!pinDynamicPangoRuntime()) {
        return QuoteError{.code = QuoteErrorCode::Internal,
                          .message = "failed to retain the Pango runtime"};
    }
    PangoFontMap* fontMap = pango_cairo_font_map_get_default();
    if (fontMap == nullptr) {
        return QuoteError{.code = QuoteErrorCode::Internal,
                          .message = "Pango font map is unavailable"};
    }
    if (pango_cairo_font_map_get_font_type(PANGO_CAIRO_FONT_MAP(fontMap)) !=
        CAIRO_FONT_TYPE_FT) {
        PangoFontMap* freetypeMap =
            pango_cairo_font_map_new_for_font_type(CAIRO_FONT_TYPE_FT);
        if (freetypeMap == nullptr) {
            return QuoteError{
                .code = QuoteErrorCode::AssetUnavailable,
                .message = "Pango FreeType font backend is unavailable"};
        }
        pango_cairo_font_map_set_default(PANGO_CAIRO_FONT_MAP(freetypeMap));
        g_object_unref(freetypeMap);
        fontMap = pango_cairo_font_map_get_default();
        if (fontMap == nullptr) {
            return QuoteError{.code = QuoteErrorCode::Internal,
                              .message = "Pango font map switch failed"};
        }
    }
    if (g_object_get_data(G_OBJECT(fontMap), registrationKey) != nullptr)
        return std::nullopt;

    for (const auto file : kBundledFontFiles) {
        const auto path = pangoFontPath(directory / file);
        GError* error = nullptr;
        if (pango_font_map_add_font_file(fontMap, path.c_str(), &error) ==
            FALSE) {
            std::string message = "failed to load bundled quote font";
            if (error != nullptr && error->message != nullptr)
                message.append(": ").append(error->message);
            if (error != nullptr)
                g_error_free(error);
            return QuoteError{.code = QuoteErrorCode::AssetUnavailable,
                              .message = std::move(message)};
        }
    }
    constexpr std::array<std::string_view, 4> requiredFamilies = {
        "Noto Sans", "Noto Sans KR", "Noto Sans Arabic", "Noto Emoji"};
    for (const auto family : requiredFamilies) {
        const std::string name(family);
        if (pango_font_map_get_family(fontMap, name.c_str()) == nullptr) {
            return QuoteError{
                .code = QuoteErrorCode::AssetUnavailable,
                .message = "bundled quote font family is unavailable: " + name};
        }
    }
    g_object_set_data(G_OBJECT(fontMap), registrationKey, GINT_TO_POINTER(1));
    return std::nullopt;
}

[[nodiscard]] compat::expected<DecodedImage, QuoteError> makeError(
    QuoteErrorCode code, std::string message) {
    return compat::unexpected<QuoteError>(
        QuoteError{.code = code, .message = std::move(message)});
}

[[nodiscard]] compat::expected<QuoteRenderResult, QuoteError> makeRenderError(
    QuoteErrorCode code, std::string message) {
    return compat::unexpected<QuoteError>(
        QuoteError{.code = code, .message = std::move(message)});
}

[[nodiscard]] bool validFinite(double value) {
    return std::isfinite(value) != 0;
}

[[nodiscard]] std::optional<Rgba> parseHexColor(std::string_view value) {
    if (value.empty() || value.front() != '#')
        return std::nullopt;
    value.remove_prefix(1);
    if (value.size() != 3 && value.size() != 4 && value.size() != 6 &&
        value.size() != 8) {
        return std::nullopt;
    }

    auto hex = [](char c) -> std::optional<unsigned> {
        if (c >= '0' && c <= '9')
            return static_cast<unsigned>(c - '0');
        if (c >= 'a' && c <= 'f')
            return static_cast<unsigned>(c - 'a' + 10);
        if (c >= 'A' && c <= 'F')
            return static_cast<unsigned>(c - 'A' + 10);
        return std::nullopt;
    };
    auto pair = [&](std::size_t index) -> std::optional<unsigned> {
        auto high = hex(value[index]);
        auto low = hex(value[index + 1]);
        if (!high || !low)
            return std::nullopt;
        return (*high << 4U) | *low;
    };

    unsigned r = 0;
    unsigned g = 0;
    unsigned b = 0;
    unsigned a = 255;
    if (value.size() <= 4) {
        auto hr = hex(value[0]);
        auto hg = hex(value[1]);
        auto hb = hex(value[2]);
        if (!hr || !hg || !hb)
            return std::nullopt;
        r = *hr * 17U;
        g = *hg * 17U;
        b = *hb * 17U;
        if (value.size() == 4) {
            auto ha = hex(value[3]);
            if (!ha)
                return std::nullopt;
            a = *ha * 17U;
        }
    } else {
        auto pr = pair(0);
        auto pg = pair(2);
        auto pb = pair(4);
        if (!pr || !pg || !pb)
            return std::nullopt;
        r = *pr;
        g = *pg;
        b = *pb;
        if (value.size() == 8) {
            auto pa = pair(6);
            if (!pa)
                return std::nullopt;
            a = *pa;
        }
    }
    constexpr double divisor = 255.0;
    return Rgba{r / divisor, g / divisor, b / divisor, a / divisor};
}

[[nodiscard]] std::vector<Rgba> backgroundColors(std::string_view value) {
    std::vector<Rgba> colors;
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] != '#')
            continue;
        std::size_t length = 1;
        while (i + length < value.size() && length <= 8) {
            const char c = value[i + length];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                  (c >= 'A' && c <= 'F'))) {
                break;
            }
            ++length;
        }
        for (std::size_t candidate :
             {std::size_t{9}, std::size_t{7}, std::size_t{5}, std::size_t{4}}) {
            if (length >= candidate) {
                if (auto color = parseHexColor(value.substr(i, candidate))) {
                    colors.push_back(*color);
                    i += candidate - 1;
                }
                break;
            }
        }
        if (colors.size() == 2)
            break;
    }
    if (colors.empty()) {
        if (value == "transparent") {
            colors.push_back(Rgba{0, 0, 0, 0});
        } else {
            colors.push_back(Rgba{0.12, 0.14, 0.16, 1});
        }
    }
    return colors;
}

void roundedRectangle(cairo_t* cr, double x, double y, double width,
                      double height, double radius) {
    radius = std::max(0.0, std::min(radius, std::min(width, height) / 2.0));
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + width - radius, y + radius, radius, -kPi / 2.0, 0);
    cairo_arc(cr, x + width - radius, y + height - radius, radius, 0,
              kPi / 2.0);
    cairo_arc(cr, x + radius, y + height - radius, radius, kPi / 2.0, kPi);
    cairo_arc(cr, x + radius, y + radius, radius, kPi, 3.0 * kPi / 2.0);
    cairo_close_path(cr);
}

void paintBackground(cairo_t* cr, int width, int height,
                     std::string_view value) {
    auto colors = backgroundColors(value);
    if (colors.size() == 1) {
        const auto& color = colors.front();
        cairo_set_source_rgba(cr, color.red, color.green, color.blue,
                              color.alpha);
        cairo_paint(cr);
        return;
    }
    cairo_pattern_t* gradient =
        cairo_pattern_create_linear(0, 0, width, height);
    cairo_pattern_add_color_stop_rgba(gradient, 0, colors[0].red,
                                      colors[0].green, colors[0].blue,
                                      colors[0].alpha);
    cairo_pattern_add_color_stop_rgba(gradient, 1, colors[1].red,
                                      colors[1].green, colors[1].blue,
                                      colors[1].alpha);
    cairo_set_source(cr, gradient);
    cairo_paint(cr);
    cairo_pattern_destroy(gradient);
}

[[nodiscard]] std::size_t utf16PositionToUtf8Byte(std::string_view text,
                                                  std::size_t units) {
    std::size_t byte = 0;
    std::size_t consumedUnits = 0;
    while (byte < text.size() && consumedUnits < units) {
        const auto first = static_cast<unsigned char>(text[byte]);
        std::size_t sequence = 1;
        std::uint32_t codepoint = first;
        if ((first & 0xE0U) == 0xC0U && byte + 1 < text.size()) {
            sequence = 2;
            codepoint = first & 0x1FU;
        } else if ((first & 0xF0U) == 0xE0U && byte + 2 < text.size()) {
            sequence = 3;
            codepoint = first & 0x0FU;
        } else if ((first & 0xF8U) == 0xF0U && byte + 3 < text.size()) {
            sequence = 4;
            codepoint = first & 0x07U;
        }
        bool valid = true;
        for (std::size_t i = 1; i < sequence; ++i) {
            const auto continuation =
                static_cast<unsigned char>(text[byte + i]);
            if ((continuation & 0xC0U) != 0x80U) {
                valid = false;
                break;
            }
            codepoint = (codepoint << 6U) | (continuation & 0x3FU);
        }
        if (!valid)
            sequence = 1;
        const std::size_t charUnits = codepoint > 0xFFFFU ? 2U : 1U;
        if (consumedUnits + charUnits > units)
            break;
        consumedUnits += charUnits;
        byte += sequence;
    }
    return byte;
}

void addEntityAttribute(PangoAttrList* list, PangoAttribute* attribute,
                        std::size_t start, std::size_t end) {
    if (!attribute || start >= end) {
        if (attribute)
            pango_attribute_destroy(attribute);
        return;
    }
    attribute->start_index = static_cast<guint>(
        std::min<std::size_t>(start, std::numeric_limits<guint>::max()));
    attribute->end_index = static_cast<guint>(
        std::min<std::size_t>(end, std::numeric_limits<guint>::max()));
    pango_attr_list_insert(list, attribute);
}

[[nodiscard]] AttrListPtr makeAttributes(
    std::string_view text, const std::vector<QuoteEntity>& entities) {
    AttrListPtr list(pango_attr_list_new(), &pango_attr_list_unref);
    for (const auto& entity : entities) {
        const auto start = utf16PositionToUtf8Byte(text, entity.offset);
        const auto end =
            utf16PositionToUtf8Byte(text, entity.offset + entity.length);
        switch (entity.type) {
            case QuoteEntityType::Bold:
                addEntityAttribute(list.get(),
                                   pango_attr_weight_new(PANGO_WEIGHT_BOLD),
                                   start, end);
                break;
            case QuoteEntityType::Italic:
                addEntityAttribute(list.get(),
                                   pango_attr_style_new(PANGO_STYLE_ITALIC),
                                   start, end);
                break;
            case QuoteEntityType::Underline:
                addEntityAttribute(
                    list.get(),
                    pango_attr_underline_new(PANGO_UNDERLINE_SINGLE), start,
                    end);
                break;
            case QuoteEntityType::Strikethrough:
                addEntityAttribute(
                    list.get(), pango_attr_strikethrough_new(TRUE), start, end);
                break;
            case QuoteEntityType::Code:
            case QuoteEntityType::Pre:
                addEntityAttribute(
                    list.get(), pango_attr_family_new("Noto Sans"), start, end);
                break;
            case QuoteEntityType::TextLink:
                addEntityAttribute(
                    list.get(),
                    pango_attr_underline_new(PANGO_UNDERLINE_SINGLE), start,
                    end);
                addEntityAttribute(
                    list.get(),
                    pango_attr_foreground_new(0x5c00, 0xa800, 0xf200), start,
                    end);
                break;
            case QuoteEntityType::CustomEmoji:
                // Preserve layout space but hide the glyph while an open
                // custom-emoji asset is painted over it. If resolution fails,
                // drawCustomEmoji deliberately leaves the original glyph.
                break;
        }
    }
    return list;
}

[[nodiscard]] LayoutPtr createLayout(cairo_t* cr, std::string_view text,
                                     const std::vector<QuoteEntity>& entities,
                                     double width, double fontPixels,
                                     bool bold = false, int maximumLines = 0) {
    LayoutPtr layout(pango_cairo_create_layout(cr), &g_object_unref);
    pango_layout_set_text(layout.get(), text.data(),
                          static_cast<int>(text.size()));
    pango_layout_set_width(
        layout.get(), static_cast<int>(std::max(1.0, width) * PANGO_SCALE));
    pango_layout_set_wrap(layout.get(), PANGO_WRAP_WORD_CHAR);
    pango_layout_set_auto_dir(layout.get(), TRUE);
    if (maximumLines > 0) {
        pango_layout_set_height(layout.get(), -maximumLines);
        pango_layout_set_ellipsize(layout.get(), PANGO_ELLIPSIZE_END);
    }
    FontPtr font(pango_font_description_new(), &pango_font_description_free);
    pango_font_description_set_family(
        font.get(), "Noto Sans, Noto Sans KR, Noto Sans Arabic, Noto Emoji");
    pango_font_description_set_absolute_size(
        font.get(), std::max(1.0, fontPixels) * PANGO_SCALE);
    if (bold)
        pango_font_description_set_weight(font.get(), PANGO_WEIGHT_BOLD);
    pango_layout_set_font_description(layout.get(), font.get());
    auto attributes = makeAttributes(text, entities);
    pango_layout_set_attributes(layout.get(), attributes.get());
    return layout;
}

[[nodiscard]] std::pair<int, int> layoutSize(PangoLayout* layout) {
    int width = 0;
    int height = 0;
    pango_layout_get_pixel_size(layout, &width, &height);
    return {width, height};
}

[[nodiscard]] std::optional<std::pair<std::uint32_t, std::uint32_t>>
probeDimensions(std::span<const std::uint8_t> bytes) {
    int webpWidth = 0;
    int webpHeight = 0;
    if (WebPGetInfo(bytes.data(), bytes.size(), &webpWidth, &webpHeight) != 0 &&
        webpWidth > 0 && webpHeight > 0) {
        return std::pair{static_cast<std::uint32_t>(webpWidth),
                         static_cast<std::uint32_t>(webpHeight)};
    }
    constexpr std::array<std::uint8_t, 8> pngSignature = {
        0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
    if (bytes.size() >= 24 &&
        std::equal(pngSignature.begin(), pngSignature.end(), bytes.begin())) {
        auto bigEndian = [&](std::size_t offset) {
            return (static_cast<std::uint32_t>(bytes[offset]) << 24U) |
                   (static_cast<std::uint32_t>(bytes[offset + 1]) << 16U) |
                   (static_cast<std::uint32_t>(bytes[offset + 2]) << 8U) |
                   static_cast<std::uint32_t>(bytes[offset + 3]);
        };
        return std::pair{bigEndian(16), bigEndian(20)};
    }
    if (bytes.size() < 4 || bytes[0] != 0xff || bytes[1] != 0xd8) {
        return std::nullopt;
    }
    std::size_t cursor = 2;
    while (cursor + 4 <= bytes.size()) {
        while (cursor < bytes.size() && bytes[cursor] == 0xff)
            ++cursor;
        if (cursor >= bytes.size())
            break;
        const std::uint8_t marker = bytes[cursor++];
        if (marker == 0xd8 || marker == 0xd9 ||
            (marker >= 0xd0 && marker <= 0xd7))
            continue;
        if (cursor + 2 > bytes.size())
            break;
        const auto length = static_cast<std::size_t>(bytes[cursor] << 8U) |
                            static_cast<std::size_t>(bytes[cursor + 1]);
        if (length < 2 || cursor + length > bytes.size())
            break;
        const bool isStartOfFrame = (marker >= 0xc0 && marker <= 0xc3) ||
                                    (marker >= 0xc5 && marker <= 0xc7) ||
                                    (marker >= 0xc9 && marker <= 0xcb) ||
                                    (marker >= 0xcd && marker <= 0xcf);
        if (isStartOfFrame && length >= 7) {
            const auto height =
                static_cast<std::uint32_t>(bytes[cursor + 3] << 8U) |
                static_cast<std::uint32_t>(bytes[cursor + 4]);
            const auto width =
                static_cast<std::uint32_t>(bytes[cursor + 5] << 8U) |
                static_cast<std::uint32_t>(bytes[cursor + 6]);
            return std::pair{width, height};
        }
        cursor += length;
    }
    return std::nullopt;
}

[[nodiscard]] compat::expected<DecodedImage, QuoteError> convertStraightBgra(
    const std::uint8_t* pixels, int width, int height,
    std::size_t sourceStride) {
    if (!pixels || width <= 0 || height <= 0 ||
        width > static_cast<int>(kMaximumDimension) ||
        height > static_cast<int>(kMaximumDimension) ||
        static_cast<std::uint64_t>(width) * height > kMaximumPixels ||
        sourceStride < static_cast<std::size_t>(width) * 4U) {
        return makeError(QuoteErrorCode::LimitExceeded,
                         "decoded image exceeds dimension or pixel limit");
    }

    DecodedImage image;
    image.width = width;
    image.height = height;
    image.stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);
    if (image.stride <= 0) {
        return makeError(QuoteErrorCode::InvalidAsset,
                         "invalid Cairo image stride");
    }
    image.cairoArgb.resize(static_cast<std::size_t>(image.stride) * height);
    for (int y = 0; y < height; ++y) {
        const auto* source =
            pixels + static_cast<std::size_t>(y) * sourceStride;
        auto* destination =
            image.cairoArgb.data() + static_cast<std::size_t>(y) * image.stride;
        for (int x = 0; x < width; ++x) {
            const std::uint8_t blue = source[x * 4];
            const std::uint8_t green = source[x * 4 + 1];
            const std::uint8_t red = source[x * 4 + 2];
            const std::uint8_t alpha = source[x * 4 + 3];
            const auto premultiply = [alpha](std::uint8_t channel) {
                return static_cast<std::uint8_t>(
                    (static_cast<unsigned>(channel) * alpha + 127U) / 255U);
            };
            // CAIRO_FORMAT_ARGB32 is a native-endian 32-bit value.  Packing
            // the value and copying it (rather than assuming BGRA byte order)
            // keeps this correct on both little- and big-endian ARM targets.
            const std::uint32_t pixel =
                (static_cast<std::uint32_t>(alpha) << 24U) |
                (static_cast<std::uint32_t>(premultiply(red)) << 16U) |
                (static_cast<std::uint32_t>(premultiply(green)) << 8U) |
                premultiply(blue);
            std::memcpy(destination + x * 4, &pixel, sizeof(pixel));
        }
    }
    return image;
}

[[nodiscard]] compat::expected<DecodedImage, QuoteError> decodePng(
    std::span<const std::uint8_t> bytes) {
    png_image image{};
    image.version = PNG_IMAGE_VERSION;
    if (png_image_begin_read_from_memory(&image, bytes.data(), bytes.size()) ==
        0) {
        return makeError(QuoteErrorCode::InvalidAsset,
                         "failed to read PNG header");
    }
    struct PngGuard {
        png_image* image;
        ~PngGuard() { png_image_free(image); }
    } guard{&image};
    if (image.width == 0 || image.height == 0 ||
        image.width > kMaximumDimension || image.height > kMaximumDimension ||
        static_cast<std::uint64_t>(image.width) * image.height >
            kMaximumPixels) {
        return makeError(QuoteErrorCode::LimitExceeded,
                         "decoded PNG exceeds dimension or pixel limit");
    }
    image.format = PNG_FORMAT_BGRA;
    const auto byteCount = PNG_IMAGE_SIZE(image);
    std::vector<std::uint8_t> pixels(byteCount);
    if (png_image_finish_read(&image, nullptr, pixels.data(), 0, nullptr) ==
        0) {
        return makeError(QuoteErrorCode::InvalidAsset,
                         "failed to decode PNG image");
    }
    return convertStraightBgra(pixels.data(), static_cast<int>(image.width),
                               static_cast<int>(image.height),
                               static_cast<std::size_t>(image.width) * 4U);
}

struct TurboJpegDeleter {
    void operator()(void* handle) const {
        if (handle)
            (void)tjDestroy(handle);
    }
};

[[nodiscard]] compat::expected<DecodedImage, QuoteError> decodeJpeg(
    std::span<const std::uint8_t> bytes) {
    std::unique_ptr<void, TurboJpegDeleter> decoder(tjInitDecompress());
    if (!decoder) {
        return makeError(QuoteErrorCode::Internal,
                         "failed to create JPEG decoder");
    }
    int width = 0;
    int height = 0;
    int subsampling = 0;
    int colorSpace = 0;
    if (tjDecompressHeader3(decoder.get(), bytes.data(),
                            static_cast<unsigned long>(bytes.size()), &width,
                            &height, &subsampling, &colorSpace) != 0 ||
        width <= 0 || height <= 0) {
        return makeError(QuoteErrorCode::InvalidAsset,
                         "failed to read JPEG header");
    }
    if (width > static_cast<int>(kMaximumDimension) ||
        height > static_cast<int>(kMaximumDimension) ||
        static_cast<std::uint64_t>(width) * height > kMaximumPixels) {
        return makeError(QuoteErrorCode::LimitExceeded,
                         "decoded JPEG exceeds dimension or pixel limit");
    }
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * height *
                                     4U);
    int flags = TJFLAG_ACCURATEDCT;
#ifdef TJFLAG_LIMITSCANS
    flags |= TJFLAG_LIMITSCANS;
#endif
    if (tjDecompress2(decoder.get(), bytes.data(),
                      static_cast<unsigned long>(bytes.size()), pixels.data(),
                      width, width * 4, height, TJPF_BGRA, flags) != 0) {
        return makeError(QuoteErrorCode::InvalidAsset,
                         "failed to decode JPEG image");
    }
    return convertStraightBgra(pixels.data(), width, height,
                               static_cast<std::size_t>(width) * 4U);
}

[[nodiscard]] compat::expected<DecodedImage, QuoteError> decodeWebP(
    std::span<const std::uint8_t> bytes) {
    int width = 0;
    int height = 0;
    std::unique_ptr<std::uint8_t, decltype(&WebPFree)> pixels(
        WebPDecodeBGRA(bytes.data(), bytes.size(), &width, &height), &WebPFree);
    if (!pixels) {
        return makeError(QuoteErrorCode::InvalidAsset,
                         "failed to decode WebP image");
    }
    return convertStraightBgra(pixels.get(), width, height,
                               static_cast<std::size_t>(width) * 4U);
}

[[nodiscard]] compat::expected<DecodedImage, QuoteError> decodeImage(
    std::span<const std::uint8_t> bytes) {
    // Prefer the narrowly scoped codecs: they enforce the preflighted
    // dimensions, and TurboJPEG's scan limit avoids progressive-JPEG CPU
    // amplification.  OpenCV remains a bounded compatibility fallback for
    // builds that provide additional decoders.
    int width = 0;
    int height = 0;
    if (WebPGetInfo(bytes.data(), bytes.size(), &width, &height) != 0)
        return decodeWebP(bytes);
    constexpr std::array<std::uint8_t, 8> pngSignature = {
        0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
    if (bytes.size() >= pngSignature.size() &&
        std::equal(pngSignature.begin(), pngSignature.end(), bytes.begin())) {
        return decodePng(bytes);
    }
    if (bytes.size() >= 2 && bytes[0] == 0xff && bytes[1] == 0xd8)
        return decodeJpeg(bytes);
#ifdef QUOTE_HAVE_OPENCV
    try {
        const cv::Mat encoded(1, static_cast<int>(bytes.size()), CV_8UC1,
                              const_cast<std::uint8_t*>(bytes.data()));
        cv::Mat decoded = cv::imdecode(encoded, cv::IMREAD_UNCHANGED);
        if (!decoded.empty() && decoded.cols > 0 && decoded.rows > 0 &&
            decoded.cols <= static_cast<int>(kMaximumDimension) &&
            decoded.rows <= static_cast<int>(kMaximumDimension) &&
            static_cast<std::uint64_t>(decoded.cols) * decoded.rows <=
                kMaximumPixels) {
            if (decoded.depth() != CV_8U) {
                cv::Mat eightBit;
                const double factor =
                    decoded.depth() == CV_16U ? 1.0 / 257.0 : 1.0;
                decoded.convertTo(
                    eightBit, CV_MAKETYPE(CV_8U, decoded.channels()), factor);
                decoded = std::move(eightBit);
            }
            cv::Mat bgra;
            if (decoded.channels() == 1) {
                cv::cvtColor(decoded, bgra, cv::COLOR_GRAY2BGRA);
            } else if (decoded.channels() == 2) {
                std::vector<cv::Mat> channels;
                cv::split(decoded, channels);
                cv::merge(std::vector<cv::Mat>{channels[0], channels[0],
                                               channels[0], channels[1]},
                          bgra);
            } else if (decoded.channels() == 3) {
                cv::cvtColor(decoded, bgra, cv::COLOR_BGR2BGRA);
            } else if (decoded.channels() == 4) {
                bgra = decoded;
            }
            if (!bgra.empty()) {
                return convertStraightBgra(bgra.data, bgra.cols, bgra.rows,
                                           bgra.step);
            }
        }
    } catch (const cv::Exception&) {
        // Report the generic unsupported-format error below.
    }
#endif
    return makeError(QuoteErrorCode::InvalidAsset,
                     "unsupported image asset format");
}

[[nodiscard]] compat::expected<DecodedImage, QuoteError> decodeAsset(
    RenderState& state, std::string_view assetId) {
    if (state.expired()) {
        return makeError(QuoteErrorCode::DeadlineExceeded,
                         "quote render deadline exceeded");
    }
    if (!state.resolver || assetId.empty()) {
        return makeError(QuoteErrorCode::AssetUnavailable,
                         "no resolver or empty asset id");
    }
    auto resolved = state.resolver->resolve(assetId);
    if (!resolved.has_value()) {
        return compat::unexpected<QuoteError>(resolved.error());
    }
    if (state.expired()) {
        return makeError(QuoteErrorCode::DeadlineExceeded,
                         "quote render deadline exceeded during asset resolve");
    }
    auto asset = std::move(resolved.value());
    if (asset.bytes.empty()) {
        return makeError(QuoteErrorCode::InvalidAsset, "asset is empty");
    }
    if (asset.bytes.size() >
        state.request.maximumSourceBytes -
            std::min(state.sourceBytes, state.request.maximumSourceBytes)) {
        return makeError(QuoteErrorCode::LimitExceeded,
                         "quote source-byte limit exceeded");
    }
    state.sourceBytes += asset.bytes.size();

    const auto dimensions = probeDimensions(asset.bytes);
    if (!dimensions || dimensions->first == 0 || dimensions->second == 0 ||
        dimensions->first > kMaximumDimension ||
        dimensions->second > kMaximumDimension ||
        static_cast<std::uint64_t>(dimensions->first) * dimensions->second >
            kMaximumPixels) {
        return makeError(QuoteErrorCode::InvalidAsset,
                         "unsupported or oversized image asset");
    }

    return decodeImage(asset.bytes);
}

void paintImage(cairo_t* cr, const DecodedImage& image, double x, double y,
                double width, double height,
                std::optional<QuoteMediaCrop> requestedCrop = std::nullopt,
                double cornerRadius = 12.0) {
    auto* mutableData = const_cast<std::uint8_t*>(image.cairoArgb.data());
    SurfacePtr surface(cairo_image_surface_create_for_data(
                           mutableData, CAIRO_FORMAT_ARGB32, image.width,
                           image.height, image.stride),
                       &cairo_surface_destroy);
    if (cairo_surface_status(surface.get()) != CAIRO_STATUS_SUCCESS)
        return;

    QuoteMediaCrop crop = requestedCrop.value_or(QuoteMediaCrop{});
    crop.x = std::clamp(crop.x, 0.0, 1.0);
    crop.y = std::clamp(crop.y, 0.0, 1.0);
    crop.width = std::clamp(crop.width, 0.001, 1.0 - crop.x);
    crop.height = std::clamp(crop.height, 0.001, 1.0 - crop.y);
    const double sourceX = crop.x * image.width;
    const double sourceY = crop.y * image.height;
    const double sourceWidth = crop.width * image.width;
    const double sourceHeight = crop.height * image.height;
    const double scale = std::max(width / sourceWidth, height / sourceHeight);
    const double paintedWidth = sourceWidth * scale;
    const double paintedHeight = sourceHeight * scale;
    const double offsetX = x + (width - paintedWidth) / 2.0;
    const double offsetY = y + (height - paintedHeight) / 2.0;

    cairo_save(cr);
    roundedRectangle(cr, x, y, width, height, cornerRadius);
    cairo_clip(cr);
    cairo_translate(cr, offsetX - sourceX * scale, offsetY - sourceY * scale);
    cairo_scale(cr, scale, scale);
    cairo_set_source_surface(cr, surface.get(), 0, 0);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BILINEAR);
    cairo_paint(cr);
    cairo_restore(cr);
}

[[nodiscard]] double mediaHeight(const QuoteMedia& media, double width,
                                 double scale) {
    if (media.width > 0 && media.height > 0) {
        const double ratio = static_cast<double>(media.height) / media.width;
        return std::clamp(width * ratio, 80.0 * scale, 220.0 * scale);
    }
    return 160.0 * scale;
}

[[nodiscard]] MessageMetrics measureMessage(cairo_t* cr,
                                            const QuoteMessage& message,
                                            double contentWidth, double scale) {
    MessageMetrics metrics;
    double height = 16.0 * scale;
    if (!message.groupWithPrevious)
        height += 24.0 * scale;
    if (message.reply) {
        const double replyPreview = message.reply->media ? 38.0 * scale : 0.0;
        metrics.replyMediaSize = replyPreview;
        auto replyLayout = createLayout(
            cr, message.reply->text, message.reply->entities,
            contentWidth - 20.0 * scale - replyPreview, 12.0 * scale, false, 2);
        const auto size = layoutSize(replyLayout.get());
        metrics.replyHeight =
            std::max({35.0 * scale, size.second + 20.0 * scale,
                      replyPreview + 4.0 * scale});
        height += metrics.replyHeight + 6.0 * scale;
    }
    if (!message.text.empty()) {
        auto layout = createLayout(cr, message.text, message.entities,
                                   contentWidth, 17.0 * scale);
        const auto size = layoutSize(layout.get());
        metrics.textHeight =
            std::max(18.0 * scale, static_cast<double>(size.second));
        height += metrics.textHeight + 6.0 * scale;
    }
    metrics.mediaHeights.reserve(message.media.size());
    for (const auto& media : message.media) {
        const double current = mediaHeight(media, contentWidth, scale);
        metrics.mediaHeights.push_back(current);
        height += current + 6.0 * scale;
    }
    if (message.voice)
        height += 54.0 * scale;
    metrics.height = height + 10.0 * scale;
    return metrics;
}

void drawVoice(cairo_t* cr, const QuoteVoice& voice, double x, double y,
               double width, double scale) {
    const double height = 44.0 * scale;
    cairo_save(cr);
    cairo_set_source_rgba(cr, 0.17, 0.22, 0.27, 0.9);
    roundedRectangle(cr, x, y, width, height, 10.0 * scale);
    cairo_fill(cr);
    const auto& waveform = voice.waveform;
    const std::size_t bars =
        std::min<std::size_t>(48, std::max<std::size_t>(12, waveform.size()));
    const double barArea = width - 58.0 * scale;
    const double barWidth = std::max(1.0, barArea / bars - 2.0 * scale);
    cairo_set_source_rgba(cr, 0.33, 0.72, 0.96, 1.0);
    for (std::size_t i = 0; i < bars; ++i) {
        const std::uint8_t value =
            waveform.empty() ? static_cast<std::uint8_t>(80 + (i * 37U) % 140U)
                             : waveform[i * waveform.size() / bars];
        const double amplitude =
            std::max(3.0 * scale, (value / 255.0) * 28.0 * scale);
        const double bx = x + 10.0 * scale + i * (barWidth + 2.0 * scale);
        const double by = y + (height - amplitude) / 2.0;
        roundedRectangle(cr, bx, by, barWidth, amplitude, barWidth / 2.0);
        cairo_fill(cr);
    }
    cairo_set_source_rgba(cr, 0.82, 0.88, 0.92, 1.0);
    auto duration = std::to_string(voice.durationSeconds / 60) + ":";
    const auto seconds = voice.durationSeconds % 60;
    if (seconds < 10)
        duration += '0';
    duration += std::to_string(seconds);
    auto layout = createLayout(cr, duration, {}, 50.0 * scale, 11.0 * scale);
    cairo_move_to(cr, x + width - 47.0 * scale, y + 14.0 * scale);
    pango_cairo_show_layout(cr, layout.get());
    cairo_restore(cr);
}

void drawCustomEmoji(cairo_t* cr, RenderState& state, PangoLayout* layout,
                     std::string_view text,
                     const std::vector<QuoteEntity>& entities, double x,
                     double y, double size) {
    for (const auto& entity : entities) {
        if (entity.type != QuoteEntityType::CustomEmoji ||
            entity.customEmojiId.empty()) {
            continue;
        }
        auto image = decodeAsset(state, entity.customEmojiId);
        if (!image.has_value())
            continue;
        const auto index = utf16PositionToUtf8Byte(text, entity.offset);
        PangoRectangle position{};
        pango_layout_index_to_pos(layout, static_cast<int>(index), &position);
        paintImage(cr, image.value(), x + position.x / PANGO_SCALE,
                   y + position.y / PANGO_SCALE, size, size, std::nullopt,
                   size / 5.0);
    }
}

void drawSenderAvatar(cairo_t* cr, RenderState& state,
                      const QuoteMessage& message, double x, double y,
                      double size) {
    const auto drawPlaceholder = [&] {
        cairo_save(cr);
        cairo_set_source_rgba(cr, 0.32, 0.57, 0.82, 1.0);
        cairo_arc(cr, x + size / 2.0, y + size / 2.0, size / 2.0, 0, 2.0 * kPi);
        cairo_fill(cr);
        cairo_restore(cr);
    };
    if (!message.avatar || !message.sender.avatar ||
        message.sender.avatar->assetId.empty()) {
        drawPlaceholder();
        return;
    }
    auto image = decodeAsset(state, message.sender.avatar->assetId);
    if (!image.has_value()) {
        drawPlaceholder();
        return;
    }
    paintImage(cr, image.value(), x, y, size, size, std::nullopt, size / 2.0);
}

[[nodiscard]] std::optional<QuoteError> drawMessage(
    cairo_t* cr, RenderState& state, const QuoteMessage& message,
    const MessageMetrics& metrics, double canvasWidth, double y, double scale) {
    const double outer = 18.0 * scale;
    const double avatarSize = 42.0 * scale;
    const double avatarGap = 8.0 * scale;
    const bool showSender = !message.groupWithPrevious;
    const double left =
        outer + (showSender && message.avatar ? avatarSize + avatarGap : 0.0);
    const double bubbleWidth = canvasWidth - left - outer;
    const double contentX = left + 14.0 * scale;
    const double contentWidth = bubbleWidth - 28.0 * scale;

    cairo_save(cr);
    cairo_set_source_rgba(cr, 0.10, 0.12, 0.15, 0.94);
    roundedRectangle(cr, left, y, bubbleWidth, metrics.height, 17.0 * scale);
    cairo_fill(cr);
    cairo_restore(cr);

    if (showSender && message.avatar) {
        drawSenderAvatar(cr, state, message, outer, y, avatarSize);
    }

    double cursorY = y + 11.0 * scale;
    if (showSender) {
        cairo_set_source_rgba(cr, 0.38, 0.72, 0.98, 1.0);
        const std::string senderName =
            message.sender.name.empty() ? "Unknown" : message.sender.name;
        auto senderLayout = createLayout(cr, senderName, {}, contentWidth,
                                         14.0 * scale, true, 1);
        cairo_move_to(cr, contentX, cursorY);
        pango_cairo_show_layout(cr, senderLayout.get());
        cursorY += 24.0 * scale;
    }
    if (message.reply) {
        cairo_set_source_rgba(cr, 0.30, 0.68, 0.93, 1.0);
        roundedRectangle(cr, contentX, cursorY, 3.0 * scale,
                         metrics.replyHeight, 1.5 * scale);
        cairo_fill(cr);
        cairo_set_source_rgba(cr, 0.45, 0.78, 0.98, 1.0);
        auto nameLayout = createLayout(
            cr,
            message.reply->sender.name.empty() ? "Reply"
                                               : message.reply->sender.name,
            {}, contentWidth - 12.0 * scale - metrics.replyMediaSize,
            11.0 * scale, true, 1);
        cairo_move_to(cr, contentX + 9.0 * scale, cursorY + 2.0 * scale);
        pango_cairo_show_layout(cr, nameLayout.get());
        cairo_set_source_rgba(cr, 0.78, 0.82, 0.86, 1.0);
        auto replyLayout =
            createLayout(cr, message.reply->text, message.reply->entities,
                         contentWidth - 12.0 * scale - metrics.replyMediaSize,
                         12.0 * scale, false, 2);
        cairo_move_to(cr, contentX + 9.0 * scale, cursorY + 16.0 * scale);
        pango_cairo_show_layout(cr, replyLayout.get());
        drawCustomEmoji(cr, state, replyLayout.get(), message.reply->text,
                        message.reply->entities, contentX + 9.0 * scale,
                        cursorY + 16.0 * scale, 13.0 * scale);
        if (message.reply->media) {
            auto image = decodeAsset(state, message.reply->media->assetId);
            if (!image.has_value())
                return image.error();
            const double preview = metrics.replyMediaSize;
            paintImage(cr, image.value(),
                       contentX + contentWidth - preview - 3.0 * scale,
                       cursorY + 2.0 * scale, preview, preview,
                       message.reply->media->crop, 6.0 * scale);
        }
        cursorY += metrics.replyHeight + 6.0 * scale;
    }
    if (!message.text.empty()) {
        cairo_set_source_rgba(cr, 0.96, 0.97, 0.98, 1.0);
        auto layout = createLayout(cr, message.text, message.entities,
                                   contentWidth, 17.0 * scale);
        cairo_move_to(cr, contentX, cursorY);
        pango_cairo_show_layout(cr, layout.get());
        drawCustomEmoji(cr, state, layout.get(), message.text, message.entities,
                        contentX, cursorY, 18.0 * scale);
        cursorY += metrics.textHeight + 6.0 * scale;
    }
    for (std::size_t i = 0; i < message.media.size(); ++i) {
        const auto& media = message.media[i];
        const double height = metrics.mediaHeights[i];
        auto image = decodeAsset(state, media.assetId);
        if (image.has_value()) {
            paintImage(
                cr, image.value(), contentX, cursorY, contentWidth, height,
                media.crop,
                media.type == QuoteMediaType::Sticker ? 0.0 : 12.0 * scale);
        } else {
            return image.error();
        }
        cursorY += height + 6.0 * scale;
    }
    if (message.voice) {
        drawVoice(cr, *message.voice, contentX, cursorY, contentWidth, scale);
    }
    return std::nullopt;
}

struct EncodedSink {
    std::vector<std::uint8_t> bytes;
    std::size_t limit{};
    std::chrono::steady_clock::time_point expiresAt;
    bool limitExceeded{};
    bool deadlineExceeded{};
    bool allocationFailed{};
};

cairo_status_t appendPng(void* closure, const unsigned char* data,
                         unsigned int length) {
    auto* sink = static_cast<EncodedSink*>(closure);
    if (std::chrono::steady_clock::now() >= sink->expiresAt) {
        sink->deadlineExceeded = true;
        return CAIRO_STATUS_WRITE_ERROR;
    }
    if (length > sink->limit - std::min(sink->bytes.size(), sink->limit)) {
        sink->limitExceeded = true;
        return CAIRO_STATUS_WRITE_ERROR;
    }
    try {
        sink->bytes.insert(sink->bytes.end(), data, data + length);
        return CAIRO_STATUS_SUCCESS;
    } catch (const std::bad_alloc&) {
        sink->allocationFailed = true;
        return CAIRO_STATUS_WRITE_ERROR;
    }
}

[[nodiscard]] compat::expected<std::vector<std::uint8_t>, QuoteError> encodePng(
    cairo_surface_t* surface, std::size_t limit,
    std::chrono::steady_clock::time_point expiresAt) {
    EncodedSink sink{.limit = limit, .expiresAt = expiresAt};
    sink.bytes.reserve(std::min<std::size_t>(limit, 1024U * 1024U));
    const auto status =
        cairo_surface_write_to_png_stream(surface, appendPng, &sink);
    if (status != CAIRO_STATUS_SUCCESS) {
        if (sink.deadlineExceeded) {
            return compat::unexpected<QuoteError>(QuoteError{
                .code = QuoteErrorCode::DeadlineExceeded,
                .message = "PNG encode deadline exceeded",
            });
        }
        if (sink.limitExceeded) {
            return compat::unexpected<QuoteError>(QuoteError{
                .code = QuoteErrorCode::LimitExceeded,
                .message = "encoded PNG exceeds output limit",
            });
        }
        return compat::unexpected<QuoteError>(QuoteError{
            .code = QuoteErrorCode::EncodeFailed,
            .message = sink.allocationFailed ? "PNG encoder allocation failed"
                                             : cairo_status_to_string(status),
        });
    }
    return std::move(sink.bytes);
}

[[nodiscard]] std::vector<std::uint8_t> straightRgba(cairo_surface_t* surface,
                                                     int width, int height) {
    cairo_surface_flush(surface);
    const auto* source = cairo_image_surface_get_data(surface);
    const int stride = cairo_image_surface_get_stride(surface);
    std::vector<std::uint8_t> rgba(static_cast<std::size_t>(width) * height *
                                   4);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            std::uint32_t pixel = 0;
            std::memcpy(&pixel,
                        source + static_cast<std::size_t>(y) * stride + x * 4,
                        sizeof(pixel));
            const auto alpha = static_cast<std::uint8_t>(pixel >> 24U);
            auto red = static_cast<std::uint8_t>(pixel >> 16U);
            auto green = static_cast<std::uint8_t>(pixel >> 8U);
            auto blue = static_cast<std::uint8_t>(pixel);
            if (alpha != 0 && alpha != 255) {
                const auto unpremultiply = [alpha](std::uint8_t channel) {
                    return static_cast<std::uint8_t>(std::min(
                        255U,
                        (static_cast<unsigned>(channel) * 255U + alpha / 2U) /
                            alpha));
                };
                red = unpremultiply(red);
                green = unpremultiply(green);
                blue = unpremultiply(blue);
            }
            const auto destination =
                (static_cast<std::size_t>(y) * width + x) * 4;
            rgba[destination] = red;
            rgba[destination + 1] = green;
            rgba[destination + 2] = blue;
            rgba[destination + 3] = alpha;
        }
    }
    return rgba;
}

int appendWebP(const std::uint8_t* data, std::size_t size,
               const WebPPicture* picture) {
    auto* sink = static_cast<EncodedSink*>(picture->custom_ptr);
    if (std::chrono::steady_clock::now() >= sink->expiresAt) {
        sink->deadlineExceeded = true;
        return 0;
    }
    if (size > sink->limit - std::min(sink->bytes.size(), sink->limit)) {
        sink->limitExceeded = true;
        return 0;
    }
    try {
        sink->bytes.insert(sink->bytes.end(), data, data + size);
        return 1;
    } catch (const std::bad_alloc&) {
        sink->allocationFailed = true;
        return 0;
    }
}

[[nodiscard]] compat::expected<std::vector<std::uint8_t>, QuoteError>
encodeWebPAttempt(std::span<const std::uint8_t> rgba, int width, int height,
                  std::size_t limit,
                  std::chrono::steady_clock::time_point expiresAt,
                  bool lossless, float quality) {
    WebPConfig config;
    bool configured =
        WebPConfigPreset(&config, WEBP_PRESET_DEFAULT, quality) != 0;
    if (configured && lossless)
        configured = WebPConfigLosslessPreset(&config, 6) != 0;
    if (!configured) {
        return compat::unexpected<QuoteError>(QuoteError{
            .code = QuoteErrorCode::Internal,
            .message = "failed to configure WebP encoder",
        });
    }
    config.thread_level = 0;
    config.exact = 1;
    if (!lossless) {
        config.method = 4;
        config.alpha_quality = 100;
    }
    if (WebPValidateConfig(&config) == 0) {
        return compat::unexpected<QuoteError>(QuoteError{
            .code = QuoteErrorCode::Internal,
            .message = "invalid WebP encoder configuration",
        });
    }

    WebPPicture picture;
    if (WebPPictureInit(&picture) == 0) {
        return compat::unexpected<QuoteError>(QuoteError{
            .code = QuoteErrorCode::Internal,
            .message = "failed to initialize WebP picture",
        });
    }
    struct PictureGuard {
        WebPPicture* picture;
        ~PictureGuard() { WebPPictureFree(picture); }
    } guard{&picture};
    picture.use_argb = 1;
    picture.width = width;
    picture.height = height;
    EncodedSink sink{.limit = limit, .expiresAt = expiresAt};
    sink.bytes.reserve(std::min<std::size_t>(limit, 1024U * 1024U));
    picture.writer = appendWebP;
    picture.custom_ptr = &sink;
    if (WebPPictureImportRGBA(&picture, rgba.data(), width * 4) == 0) {
        return compat::unexpected<QuoteError>(QuoteError{
            .code = QuoteErrorCode::EncodeFailed,
            .message = "failed to import WebP pixels",
        });
    }
    if (WebPEncode(&config, &picture) != 0)
        return std::move(sink.bytes);
    if (sink.deadlineExceeded) {
        return compat::unexpected<QuoteError>(QuoteError{
            .code = QuoteErrorCode::DeadlineExceeded,
            .message = "WebP encode deadline exceeded",
        });
    }
    if (sink.limitExceeded) {
        return compat::unexpected<QuoteError>(QuoteError{
            .code = QuoteErrorCode::LimitExceeded,
            .message = "encoded WebP exceeds output limit",
        });
    }
    return compat::unexpected<QuoteError>(QuoteError{
        .code = QuoteErrorCode::EncodeFailed,
        .message = sink.allocationFailed ? "WebP encoder allocation failed"
                                         : "WebP encoder failed",
    });
}

[[nodiscard]] compat::expected<std::vector<std::uint8_t>, QuoteError>
encodeWebP(cairo_surface_t* surface, int width, int height, std::size_t limit,
           std::chrono::steady_clock::time_point expiresAt) {
    if (std::chrono::steady_clock::now() >= expiresAt) {
        return compat::unexpected<QuoteError>(QuoteError{
            .code = QuoteErrorCode::DeadlineExceeded,
            .message = "WebP encode deadline exceeded",
        });
    }
    const auto rgba = straightRgba(surface, width, height);
    if (std::chrono::steady_clock::now() >= expiresAt) {
        return compat::unexpected<QuoteError>(QuoteError{
            .code = QuoteErrorCode::DeadlineExceeded,
            .message = "WebP encode deadline exceeded",
        });
    }
    auto encoded =
        encodeWebPAttempt(rgba, width, height, limit, expiresAt, true, 100.0F);
    if (encoded.has_value() ||
        encoded.error().code != QuoteErrorCode::LimitExceeded) {
        return encoded;
    }
    for (float quality :
         {90.0F, 80.0F, 70.0F, 60.0F, 50.0F, 40.0F, 30.0F, 20.0F}) {
        auto attempt = encodeWebPAttempt(rgba, width, height, limit, expiresAt,
                                         false, quality);
        if (attempt.has_value() ||
            attempt.error().code != QuoteErrorCode::LimitExceeded) {
            return attempt;
        }
    }
    return compat::unexpected<QuoteError>(QuoteError{
        .code = QuoteErrorCode::LimitExceeded,
        .message = "encoded WebP exceeds output limit",
    });
}

[[nodiscard]] bool validUtf8(std::string_view value) {
    return g_utf8_validate(value.data(), static_cast<gssize>(value.size()),
                           nullptr) != FALSE;
}

[[nodiscard]] std::size_t utf16Length(std::string_view value) {
    std::size_t units = 0;
    for (std::size_t offset = 0; offset < value.size();) {
        const auto first = static_cast<unsigned char>(value[offset]);
        std::size_t sequence = 1;
        if ((first & 0xE0U) == 0xC0U)
            sequence = 2;
        else if ((first & 0xF0U) == 0xE0U)
            sequence = 3;
        else if ((first & 0xF8U) == 0xF0U)
            sequence = 4;
        units += sequence == 4 ? 2U : 1U;
        offset += sequence;
    }
    return units;
}

[[nodiscard]] std::optional<QuoteError> validateEntities(
    std::string_view text, const std::vector<QuoteEntity>& entities,
    std::size_t& entityCount, std::size_t& metadataBytes) {
    if (entities.size() >
        kMaximumEntities - std::min(entityCount, kMaximumEntities)) {
        return QuoteError{.code = QuoteErrorCode::LimitExceeded,
                          .message = "too many quote entities"};
    }
    entityCount += entities.size();
    const auto textUnits = utf16Length(text);
    for (const auto& entity : entities) {
        if (entity.offset > textUnits || entity.length == 0 ||
            entity.length > textUnits - entity.offset) {
            return QuoteError{.code = QuoteErrorCode::InvalidRequest,
                              .message = "entity range is outside its text"};
        }
        switch (entity.type) {
            case QuoteEntityType::Bold:
            case QuoteEntityType::Italic:
            case QuoteEntityType::Underline:
            case QuoteEntityType::Strikethrough:
            case QuoteEntityType::Code:
            case QuoteEntityType::Pre:
                break;
            case QuoteEntityType::TextLink:
                if (entity.url.empty()) {
                    return QuoteError{.code = QuoteErrorCode::InvalidRequest,
                                      .message = "text-link entity has no URL"};
                }
                break;
            case QuoteEntityType::CustomEmoji:
                if (entity.customEmojiId.empty()) {
                    return QuoteError{
                        .code = QuoteErrorCode::InvalidRequest,
                        .message = "custom-emoji entity has no asset ID"};
                }
                break;
            default:
                return QuoteError{.code = QuoteErrorCode::InvalidRequest,
                                  .message = "unknown quote entity type"};
        }
        for (const auto value : {std::string_view(entity.url),
                                 std::string_view(entity.customEmojiId)}) {
            if (!validUtf8(value) ||
                value.size() >
                    kMaximumMetadataBytes -
                        std::min(metadataBytes, kMaximumMetadataBytes)) {
                return QuoteError{.code = QuoteErrorCode::LimitExceeded,
                                  .message = "quote metadata exceeds limit"};
            }
            metadataBytes += value.size();
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<QuoteError> validateMedia(
    const QuoteMedia& media, std::size_t& metadataBytes) {
    if (media.assetId.empty() || media.assetId.size() > kMaximumAssetIdBytes ||
        !validUtf8(media.assetId)) {
        return QuoteError{.code = QuoteErrorCode::InvalidRequest,
                          .message = "media asset ID is invalid"};
    }
    if (media.assetId.size() >
        kMaximumMetadataBytes -
            std::min(metadataBytes, kMaximumMetadataBytes)) {
        return QuoteError{.code = QuoteErrorCode::LimitExceeded,
                          .message = "quote metadata exceeds limit"};
    }
    metadataBytes += media.assetId.size();
    switch (media.type) {
        case QuoteMediaType::Photo:
        case QuoteMediaType::Sticker:
        case QuoteMediaType::Video:
        case QuoteMediaType::Animation:
            break;
        default:
            return QuoteError{.code = QuoteErrorCode::InvalidRequest,
                              .message = "unknown quote media type"};
    }
    if (media.crop &&
        (!validFinite(media.crop->x) || !validFinite(media.crop->y) ||
         !validFinite(media.crop->width) || !validFinite(media.crop->height) ||
         media.crop->x < 0 || media.crop->y < 0 || media.crop->width <= 0 ||
         media.crop->height <= 0 ||
         media.crop->x + media.crop->width > 1.000001 ||
         media.crop->y + media.crop->height > 1.000001)) {
        return QuoteError{.code = QuoteErrorCode::InvalidRequest,
                          .message = "invalid normalized media crop"};
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<QuoteError> validateRequest(
    const QuoteRenderRequest& request) {
    switch (request.type) {
        case QuoteOutputType::Quote:
        case QuoteOutputType::Image:
        case QuoteOutputType::Stories:
            break;
        default:
            return QuoteError{.code = QuoteErrorCode::InvalidRequest,
                              .message = "unknown quote output type"};
    }
    switch (request.format) {
        case QuoteOutputFormat::Png:
        case QuoteOutputFormat::WebP:
            break;
        default:
            return QuoteError{.code = QuoteErrorCode::UnsupportedFormat,
                              .message = "unknown quote output format"};
    }
    switch (request.emojiBrand) {
        case QuoteEmojiBrand::Open:
        case QuoteEmojiBrand::Apple:
        case QuoteEmojiBrand::Google:
        case QuoteEmojiBrand::Twitter:
        case QuoteEmojiBrand::Facebook:
        case QuoteEmojiBrand::Samsung:
        case QuoteEmojiBrand::JoyPixels:
            break;
        default:
            return QuoteError{.code = QuoteErrorCode::InvalidRequest,
                              .message = "unknown emoji brand selector"};
    }
    if (request.messages.empty() ||
        request.messages.size() > kMaximumMessages) {
        return QuoteError{
            .code = QuoteErrorCode::InvalidRequest,
            .message = "quote requires between 1 and 20 messages"};
    }
    if (!validFinite(request.scale) || request.scale < 0.5 ||
        request.scale > 3.0) {
        return QuoteError{.code = QuoteErrorCode::InvalidRequest,
                          .message = "scale must be between 0.5 and 3.0"};
    }
    if (request.deadline <= std::chrono::milliseconds::zero() ||
        request.deadline > kMaximumDeadline) {
        return QuoteError{.code = QuoteErrorCode::InvalidRequest,
                          .message = "deadline must be between 1 ms and 5 min"};
    }
    constexpr std::size_t absoluteByteLimit = 64U * 1024U * 1024U;
    if (request.maximumSourceBytes == 0 || request.maximumEncodedBytes == 0 ||
        request.maximumSourceBytes > absoluteByteLimit ||
        request.maximumEncodedBytes > absoluteByteLimit) {
        return QuoteError{
            .code = QuoteErrorCode::InvalidRequest,
            .message = "byte limits must be between 1 and 64 MiB"};
    }
    if (request.telegramSticker && request.format != QuoteOutputFormat::WebP) {
        return QuoteError{.code = QuoteErrorCode::UnsupportedFormat,
                          .message = "Telegram static stickers require WebP"};
    }
    if (request.background.size() > kMaximumBackgroundBytes ||
        !validUtf8(request.background)) {
        return QuoteError{.code = QuoteErrorCode::InvalidRequest,
                          .message = "quote background is invalid"};
    }
    std::size_t totalText = 0;
    std::size_t metadataBytes = request.background.size();
    std::size_t entityCount = 0;
    const auto addString =
        [&](std::string_view value, std::size_t limit,
            std::size_t& total) -> std::optional<QuoteError> {
        if (!validUtf8(value)) {
            return QuoteError{.code = QuoteErrorCode::InvalidRequest,
                              .message = "quote contains invalid UTF-8"};
        }
        if (value.size() > limit - std::min(total, limit)) {
            return QuoteError{.code = QuoteErrorCode::LimitExceeded,
                              .message = "quote string budget exceeded"};
        }
        total += value.size();
        return std::nullopt;
    };
    for (std::size_t index = 0; index < request.messages.size(); ++index) {
        const auto& message = request.messages[index];
        if (index == 0 && message.groupWithPrevious) {
            return QuoteError{.code = QuoteErrorCode::InvalidRequest,
                              .message = "first message cannot be grouped"};
        }
        if (auto error = addString(message.text, kMaximumTextBytes, totalText))
            return error;
        for (const auto value : {std::string_view(message.sender.name),
                                 std::string_view(message.sender.username)}) {
            if (auto error =
                    addString(value, kMaximumMetadataBytes, metadataBytes))
                return error;
        }
        if (message.sender.avatar) {
            const auto& id = message.sender.avatar->assetId;
            if (id.empty() || id.size() > kMaximumAssetIdBytes) {
                return QuoteError{.code = QuoteErrorCode::InvalidRequest,
                                  .message = "avatar asset ID is invalid"};
            }
            if (auto error =
                    addString(id, kMaximumMetadataBytes, metadataBytes))
                return error;
        }
        if (auto error = validateEntities(message.text, message.entities,
                                          entityCount, metadataBytes))
            return error;
        if (message.media.size() > kMaximumMediaPerMessage) {
            return QuoteError{.code = QuoteErrorCode::LimitExceeded,
                              .message = "too many media assets in a message"};
        }
        for (const auto& media : message.media) {
            if (auto error = validateMedia(media, metadataBytes))
                return error;
        }
        if (message.reply) {
            if (auto error = addString(message.reply->text, kMaximumTextBytes,
                                       totalText))
                return error;
            for (const auto value :
                 {std::string_view(message.reply->sender.name),
                  std::string_view(message.reply->sender.username)}) {
                if (auto error =
                        addString(value, kMaximumMetadataBytes, metadataBytes))
                    return error;
            }
            if (auto error = validateEntities(message.reply->text,
                                              message.reply->entities,
                                              entityCount, metadataBytes))
                return error;
            if (message.reply->media) {
                if (auto error =
                        validateMedia(*message.reply->media, metadataBytes))
                    return error;
            }
        }
        if (message.voice &&
            message.voice->waveform.size() > kMaximumWaveformBytes) {
            return QuoteError{.code = QuoteErrorCode::LimitExceeded,
                              .message = "voice waveform exceeds limit"};
        }
    }
    return std::nullopt;
}

}  // namespace

QuoteRenderer::QuoteRenderer(std::filesystem::path fontDirectory,
                             QuoteAssetResolver* resolver)
    : fontDirectory_(std::move(fontDirectory)), resolver_(resolver) {}

compat::expected<QuoteRenderResult, QuoteError> QuoteRenderer::render(
    const QuoteRenderRequest& request) const {
    if (const auto error = validateRequest(request)) {
        return compat::unexpected<QuoteError>(*error);
    }

    try {
        const auto started = std::chrono::steady_clock::now();
        if (const auto error = registerBundledFonts(fontDirectory_)) {
            return compat::unexpected<QuoteError>(*error);
        }
        RenderState state{.request = request,
                          .resolver = resolver_,
                          .started = started,
                          .expiresAt = started + request.deadline};
        const double scale = request.scale;
        std::uint32_t width = request.width;
        std::uint32_t height = request.height;
        switch (request.type) {
            case QuoteOutputType::Quote:
                if (width == 0)
                    width = 512;
                break;
            case QuoteOutputType::Image:
                if (width == 0)
                    width = 1200;
                if (height == 0)
                    height = 630;
                break;
            case QuoteOutputType::Stories:
                if (width == 0)
                    width = 1080;
                if (height == 0)
                    height = 1920;
                break;
        }
        if (request.telegramSticker)
            width = 512;
        const auto minimumWidth =
            static_cast<std::uint32_t>(std::ceil(160.0 * scale));
        const auto minimumHeight =
            static_cast<std::uint32_t>(std::ceil(64.0 * scale));
        if (width < minimumWidth || width > kMaximumDimension ||
            height > kMaximumDimension ||
            (height != 0 && height < minimumHeight)) {
            return makeRenderError(QuoteErrorCode::LimitExceeded,
                                   "canvas dimension exceeds limit");
        }

        SurfacePtr scratchSurface(
            cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1),
            &cairo_surface_destroy);
        CairoPtr scratch(cairo_create(scratchSurface.get()), &cairo_destroy);
        if (cairo_status(scratch.get()) != CAIRO_STATUS_SUCCESS) {
            return makeRenderError(
                QuoteErrorCode::Internal,
                "failed to create Pango measurement surface");
        }
        const double showAvatarOffset = 50.0 * scale;
        const double contentWidth = std::max(
            64.0, static_cast<double>(width) - 64.0 * scale - showAvatarOffset);
        std::vector<MessageMetrics> metrics;
        metrics.reserve(request.messages.size());
        double contentHeight = 24.0 * scale;
        for (const auto& message : request.messages) {
            metrics.push_back(
                measureMessage(scratch.get(), message, contentWidth, scale));
            contentHeight += metrics.back().height + 8.0 * scale;
        }
        contentHeight += 16.0 * scale;
        if (request.type == QuoteOutputType::Quote && height == 0) {
            if (!validFinite(contentHeight) ||
                contentHeight > kMaximumDimension) {
                return makeRenderError(QuoteErrorCode::LimitExceeded,
                                       "quote content exceeds canvas limit");
            }
            height = static_cast<std::uint32_t>(std::ceil(contentHeight));
        }
        if (height == 0)
            height = 1;
        if (height > kMaximumDimension ||
            static_cast<std::uint64_t>(width) * height > kMaximumPixels) {
            return makeRenderError(QuoteErrorCode::LimitExceeded,
                                   "canvas exceeds dimension or pixel limit");
        }
        if (contentHeight > height && request.type == QuoteOutputType::Quote &&
            request.height != 0) {
            return makeRenderError(QuoteErrorCode::LimitExceeded,
                                   "quote content does not fit the canvas");
        }

        SurfacePtr surface(cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                                      static_cast<int>(width),
                                                      static_cast<int>(height)),
                           &cairo_surface_destroy);
        if (cairo_surface_status(surface.get()) != CAIRO_STATUS_SUCCESS) {
            return makeRenderError(QuoteErrorCode::Internal,
                                   "failed to allocate quote canvas");
        }
        CairoPtr cr(cairo_create(surface.get()), &cairo_destroy);
        if (cairo_status(cr.get()) != CAIRO_STATUS_SUCCESS) {
            return makeRenderError(QuoteErrorCode::Internal,
                                   "failed to create quote drawing context");
        }
        paintBackground(cr.get(), static_cast<int>(width),
                        static_cast<int>(height), request.background);

        double y =
            request.type == QuoteOutputType::Quote
                ? 16.0 * scale
                : std::max(16.0 * scale,
                           (static_cast<double>(height) - contentHeight) / 2.0);
        for (std::size_t i = 0; i < request.messages.size(); ++i) {
            if (state.expired()) {
                return makeRenderError(QuoteErrorCode::DeadlineExceeded,
                                       "quote render deadline exceeded");
            }
            if (auto error = drawMessage(cr.get(), state, request.messages[i],
                                         metrics[i], width, y, scale)) {
                return compat::unexpected<QuoteError>(*error);
            }
            y += metrics[i].height + 8.0 * scale;
        }
        cairo_surface_flush(surface.get());
        if (cairo_surface_status(surface.get()) != CAIRO_STATUS_SUCCESS) {
            return makeRenderError(QuoteErrorCode::Internal,
                                   "quote drawing surface failed");
        }

        SurfacePtr scaledSurface(nullptr, &cairo_surface_destroy);
        cairo_surface_t* outputSurface = surface.get();
        std::uint32_t outputWidth = width;
        std::uint32_t outputHeight = height;
        if (request.telegramSticker && (width > 512 || height > 512)) {
            const double outputScale = std::min(512.0 / width, 512.0 / height);
            outputWidth = std::max<std::uint32_t>(
                1,
                static_cast<std::uint32_t>(std::lround(width * outputScale)));
            outputHeight = std::max<std::uint32_t>(
                1,
                static_cast<std::uint32_t>(std::lround(height * outputScale)));
            if (width >= height)
                outputWidth = 512;
            if (height >= width)
                outputHeight = 512;
            scaledSurface.reset(cairo_image_surface_create(
                CAIRO_FORMAT_ARGB32, static_cast<int>(outputWidth),
                static_cast<int>(outputHeight)));
            if (cairo_surface_status(scaledSurface.get()) !=
                CAIRO_STATUS_SUCCESS) {
                return makeRenderError(
                    QuoteErrorCode::Internal,
                    "failed to allocate sticker output canvas");
            }
            CairoPtr scaled(cairo_create(scaledSurface.get()), &cairo_destroy);
            cairo_set_operator(scaled.get(), CAIRO_OPERATOR_SOURCE);
            cairo_scale(scaled.get(), outputScale, outputScale);
            cairo_set_source_surface(scaled.get(), surface.get(), 0, 0);
            cairo_pattern_set_filter(cairo_get_source(scaled.get()),
                                     CAIRO_FILTER_BEST);
            cairo_paint(scaled.get());
            cairo_surface_flush(scaledSurface.get());
            if (cairo_surface_status(scaledSurface.get()) !=
                CAIRO_STATUS_SUCCESS) {
                return makeRenderError(QuoteErrorCode::Internal,
                                       "sticker scaling surface failed");
            }
            outputSurface = scaledSurface.get();
        }

        const std::size_t outputLimit =
            request.telegramSticker
                ? std::min(request.maximumEncodedBytes, kTelegramStickerBytes)
                : request.maximumEncodedBytes;
        QuoteRenderResult result;
        if (request.format == QuoteOutputFormat::Png) {
            result.mimeType = "image/png";
            result.fileName = "quote.png";
        } else {
            result.mimeType = "image/webp";
            result.fileName = "quote.webp";
        }
        auto encoded =
            request.format == QuoteOutputFormat::Png
                ? encodePng(outputSurface, outputLimit, state.expiresAt)
                : encodeWebP(outputSurface, static_cast<int>(outputWidth),
                             static_cast<int>(outputHeight), outputLimit,
                             state.expiresAt);
        if (!encoded.has_value()) {
            return compat::unexpected<QuoteError>(encoded.error());
        }
        result.bytes = std::move(encoded.value());
        result.width = outputWidth;
        result.height = outputHeight;
        LOG(INFO) << "Native quote rendered " << outputWidth << 'x'
                  << outputHeight << " in "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now() - state.started)
                         .count()
                  << "ms; source=" << state.sourceBytes
                  << " bytes, output=" << result.bytes.size() << " bytes";
        return result;
    } catch (const std::bad_alloc&) {
        return makeRenderError(QuoteErrorCode::LimitExceeded,
                               "quote render allocation failed");
    } catch (const std::exception& error) {
        return makeRenderError(QuoteErrorCode::Internal, error.what());
    } catch (...) {
        return makeRenderError(QuoteErrorCode::Internal,
                               "unknown quote render failure");
    }
}

}  // namespace quote
