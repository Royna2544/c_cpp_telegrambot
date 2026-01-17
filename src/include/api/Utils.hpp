#pragma once

#include <api/types/Animation.hpp>
#include <api/types/Chat.hpp>
#include <api/types/PhotoSize.hpp>
#include <api/types/Sticker.hpp>
#include <api/types/Video.hpp>
#include <api/types/ReplyParameters.hpp>

#include <string>
#include <utility>

/**
 * @brief A utility class to hold Telegram media IDs and unique IDs.
 *
 * This class encapsulates the file ID and file unique ID of Telegram media
 * such as animations, photos, and videos. It provides methods to compare media
 * IDs, check for emptiness, and construct instances from different media types.
 */
struct MediaIds {
    std::string id;        //!< The Telegram media ID.
    std::string uniqueid;  //!< The Telegram media unique ID.

    /**
     * @brief Constructs a MediaIds object from a api::types::Animation object.
     *
     * @param animation The api::types::Animation object from which to extract the
     * media ID and unique ID.
     */
    explicit MediaIds(const api::types::Animation& animation)
        : id(animation.fileId), uniqueid(animation.fileUniqueId) {}

    /**
     * @brief Constructs a MediaIds object from a api::types::PhotoSize object.
     *
     * @param photo The api::types::PhotoSize object from which to extract the
     * media ID and unique ID.
     */
    explicit MediaIds(const api::types::PhotoSize& photo)
        : id(photo.fileId), uniqueid(photo.fileUniqueId) {}

    /**
     * @brief Constructs a MediaIds object from a api::types::Video object.
     *
     * @param video The api::types::Video object from which to extract the
     * media ID and unique ID.
     */
    explicit MediaIds(const api::types::Video& video)
        : id(video.fileId), uniqueid(video.fileUniqueId) {}

    explicit MediaIds(const api::types::Sticker& sticker)
        : id(sticker.fileId), uniqueid(sticker.fileUniqueId) {}

    /**
     * @brief Default constructor. Initializes the media ID and unique ID to
     * empty strings.
     */
    MediaIds() = default;

    /**
     * @brief Constructs a MediaIds object with the given IDs.
     *
     * @param id The Telegram media ID.
     * @param uniqueid The Telegram media unique ID.
     */
    explicit MediaIds(std::string id, std::string uniqueid)
        : id(std::move(id)), uniqueid(std::move(uniqueid)) {}

    /**
     * @brief Compares two MediaIds objects for equality.
     *
     * @param other The MediaIds object to compare with this one.
     * @return True if the IDs and unique IDs of both objects are equal, false
     * otherwise.
     */
    auto operator<=>(const MediaIds& other) const { return id <=> other.id; }

    /**
     * @brief Checks if the MediaIds object is empty.
     *
     * @return True if both the media ID and unique ID are empty strings, false
     * otherwise.
     */
    [[nodiscard]] bool empty() const { return id.empty() && uniqueid.empty(); }
};

// Helper to remove duplicate overloads for ChatId and MessageTypes
struct ChatIds {
    ChatIds(api::types::Chat::id_type id) : _id(id) {}
    ChatIds(const api::types::Chat& chat) : _id(chat.id) {}
    operator api::types::Chat::id_type() const { return _id; }
    api::types::Chat::id_type _id;
};

struct ReplyParamsToMsgTid {
    explicit ReplyParamsToMsgTid(
        const std::optional<api::types::ReplyParameters>& replyParameters) {
        if (replyParameters && replyParameters->messageThreadId) {
            tid = replyParameters->messageThreadId;
        }
    }
    operator std::optional<api::types::Message::messageThreadId_type>() const { return tid; }

    std::optional<api::types::Message::messageThreadId_type> tid;
};
