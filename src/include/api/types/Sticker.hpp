#pragma once

#include <api/types/File.hpp>
#include <api/types/MaskPosition.hpp>
#include <api/types/PhotoSize.hpp>
#include <cstdint>
#include <optional>
#include <string>

namespace api::types {

/**
 * @brief This object represents a sticker.
 */
struct Sticker {
    /**
     * @brief Enum of possible types of a sticker.
     */
    enum class Type { Regular, Mask, CustomEmoji };

    /**
     * @brief Identifier for this file, which can be used to download or reuse
     * the file
     */
    std::string fileId;

    /**
     * @brief Unique identifier for this file, which is supposed to be the same
     * over time and for different bots. Can't be used to download or reuse the
     * file.
     */
    std::string fileUniqueId;

    /**
     * @brief Type of the sticker, currently one of “regular”, “mask”,
     * “custom_emoji”. The type of the sticker is independent from its format,
     * which is determined by the fields isAnimated and isVideo.
     */
    Type type;

    /**
     * @brief Sticker width
     */
    std::int32_t width;

    /**
     * @brief Sticker height
     */
    std::int32_t height;

    /**
     * @brief True, if the sticker is animated
     */
    bool isAnimated;

    /**
     * @brief True, if the sticker is a video sticker
     */
    bool isVideo;

    /**
     * @brief Optional. Sticker thumbnail in the .WEBP or .JPG format
     */
    std::optional<PhotoSize> thumbnail;

    /**
     * @brief Optional. Emoji associated with the sticker
     */
    std::optional<std::string> emoji;

    /**
     * @brief Optional. Name of the sticker set to which the sticker belongs
     */
    std::optional<std::string> setName;

    /**
     * @brief Optional. For premium regular stickers, premium animation for the
     * sticker
     */
    std::optional<File> premiumAnimation;

    /**
     * @brief Optional. For mask stickers, the position where the mask should be
     * placed
     */
    std::optional<MaskPosition> maskPosition;

    /**
     * @brief Optional. For custom emoji stickers, unique identifier of the
     * custom emoji
     */
    std::optional<std::string> customEmojiId;

    /**
     * @brief Optional. True, if the sticker must be repainted to a text color
     * in messages, the color of the Telegram Premium badge in emoji status,
     * white color on chat photos, or another appropriate color in other places
     */
    std::optional<bool> needsRepainting;

    /**
     * @brief Optional. File size in bytes
     */
    std::optional<std::int32_t> fileSize;
};

}  // namespace api::types