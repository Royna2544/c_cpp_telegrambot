#pragma once

#include <string>
#include <vector>

#include "api/types/PhotoSize.hpp"
#include "api/types/Sticker.hpp"

namespace api::types {

/**
 * @brief This object represents a sticker set.
 */
struct StickerSet {
    /**
     * @brief Enum of possible types of a sticker.
     */
    enum class Type { Regular, Mask, CustomEmoji };

    /**
     * @brief Sticker set name
     */
    std::string name;

    /**
     * @brief Sticker set title
     */
    std::string title;

    /**
     * @brief Type of stickers in the set, currently one of Type::Regular,
     * Type::Mask, Type::CustomEmoji”
     */
    Type stickerType;

    /**
     * @brief List of all set stickers
     */
    std::vector<Sticker> stickers;

    /**
     * @brief Optional. Sticker set thumbnail in the .WEBP, .TGS, or .WEBM
     * format
     */
    std::optional<PhotoSize> thumbnail;
};

}  // namespace api::types