#pragma once

#include <api/typedefs.h>
#include <tgbot/types/Animation.h>
#include <tgbot/types/Chat.h>
#include <tgbot/types/PhotoSize.h>
#include <tgbot/types/Sticker.h>
#include <tgbot/types/Video.h>

#include "ReplyParametersExt.hpp"
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
     * @brief Constructs a MediaIds object from a TgBot::Animation object.
     *
     * @param animation The TgBot::Animation object from which to extract the
     * media ID and unique ID.
     */
    explicit MediaIds(const TgBot::Animation::Ptr& animation)
        : id(animation->fileId), uniqueid(animation->fileUniqueId) {}

    /**
     * @brief Constructs a MediaIds object from a TgBot::PhotoSize object.
     *
     * @param photo The TgBot::PhotoSize object from which to extract the
     * media ID and unique ID.
     */
    explicit MediaIds(const TgBot::PhotoSize::Ptr& photo)
        : id(photo->fileId), uniqueid(photo->fileUniqueId) {}

    /**
     * @brief Constructs a MediaIds object from a TgBot::Video object.
     *
     * @param video The TgBot::Video object from which to extract the
     * media ID and unique ID.
     */
    explicit MediaIds(const TgBot::Video::Ptr& video)
        : id(video->fileId), uniqueid(video->fileUniqueId) {}

    explicit MediaIds(const TgBot::Sticker::Ptr& sticker)
        : id(sticker->fileId), uniqueid(sticker->fileUniqueId) {}

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
    ChatIds(ChatId id) : _id(id) {}
    ChatIds(const TgBot::Chat::Ptr& chat) : _id(chat->id) {}
    operator ChatId() const { return _id; }
    ChatId _id;
};

struct ReplyParamsToMsgTid {
    explicit ReplyParamsToMsgTid(
        const ReplyParametersExt::Ptr& replyParameters) {
        if (replyParameters && replyParameters->messageThreadId) {
            tid = replyParameters->messageThreadId;
        }
    }
    operator std::optional<MessageThreadId>() const { return tid; }

    std::optional<MessageThreadId> tid;
};
