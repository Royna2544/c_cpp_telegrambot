#pragma once

#include <string>

namespace api::types {

/**
 * @brief This object represents a chat photo.
 */
struct ChatPhoto {
    /**
     * @brief File identifier of small (160x160) chat photo.
     * This fileId can be used only for photo download and only for as long as
     * the photo is not changed.
     */
    std::string smallFileId;

    /**
     * @brief Unique file identifier of small (160x160) chat photo, which is
     * supposed to be the same over time and for different bots. Can't be used
     * to download or reuse the file.
     */
    std::string smallFileUniqueId;

    /**
     * @brief File identifier of big (640x640) chat photo.
     * This fileId can be used only for photo download and only for as long as
     * the photo is not changed.
     */
    std::string bigFileId;

    /**
     * @brief Unique file identifier of big (640x640) chat photo, which is
     * supposed to be the same over time and for different bots. Can't be used
     * to download or reuse the file.
     */
    std::string bigFileUniqueId;
};

}  // namespace api::types