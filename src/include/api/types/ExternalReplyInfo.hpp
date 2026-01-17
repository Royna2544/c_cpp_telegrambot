#pragma once

#include <api/types/Animation.hpp>
#include <api/types/Chat.hpp>
#include <api/types/Document.hpp>
#include <api/types/Dice.hpp>
#include <api/types/LinkPreviewOptions.hpp>
#include <api/types/MessageOrigins.hpp>
#include <api/types/PhotoSize.hpp>
#include <api/types/Poll.hpp>
#include <api/types/Sticker.hpp>
#include <api/types/Story.hpp>
#include <api/types/Video.hpp>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace api::types {

/**
 * @brief This object contains information about a message that is being replied
 * to, which may come from another chat or forum topic.
 */
struct ExternalReplyInfo {
    /**
     * @brief Origin of the message replied to by the given message
     */
    MessageOrigin origin;

    /**
     * @brief Optional. Chat the original message belongs to.
     *
     * Available only if the chat is a supergroup or a channel.
     */
    std::optional<Chat> chat;

    /**
     * @brief Optional. Unique message identifier inside the original chat.
     *
     * Available only if the original chat is a supergroup or a channel.
     */
    std::optional<std::int32_t> messageId;

    /**
     * @brief Optional. Options used for link preview generation for the
     * original message, if it is a text message
     */
    std::optional<LinkPreviewOptions> linkPreviewOptions;

    /**
     * @brief Optional. Message is an animation, information about the animation
     */
    std::optional<Animation> animation;

    /**
     * @brief Optional. Message is an audio file, information about the file
     * 
     * API_REMOVED: Unused in this project [SubCategories.Audio].
     * FieldType: Audio
     * FieldName: audio
     */

    /**
     * @brief Optional. Message is a general file, information about the file
     */
    std::optional<Document> document;

    /**
     * @brief Optional. Message is a photo, available sizes of the photo
     */
    std::optional<std::vector<PhotoSize>> photo;

    /**
     * @brief Optional. Message is a sticker, information about the sticker
     */
    std::optional<Sticker> sticker;

    /**
     * @brief Optional. Message is a forwarded story
     */
    std::optional<Story> story;

    /**
     * @brief Optional. Message is a video, information about the video
     */
    std::optional<Video> video;

    /**
     * @brief Optional. Message is a [video
     * note](https://telegram.org/blog/video-messages-and-telescope),
     * information about the video message
     * 
     * API_REMOVED: Unused in this project [SubCategories.VideoNote].
     * FieldType: VideoNote
     * FieldName: videoNote
     */

    /**
     * @brief Optional. Message is a voice message, information about the file
     * 
     * API_REMOVED: Unused in this project [SubCategories.Voice].
     * FieldType: Voice
     * FieldName: voice
     */

    /**
     * @brief Optional. True, if the message media is covered by a spoiler
     * animation
     */
    std::optional<bool> hasMediaSpoiler;

    /**
     * @brief Optional. Message is a shared contact, information about the
     * contact
     * 
     * API_REMOVED: Unused in this project [SubCategories.Contact].
     * FieldType: Contact
     * FieldName: contact
     */

    /**
     * @brief Optional. Message is a dice with random value
     */
    std::optional<Dice> dice;

    /**
     * @brief Optional. Message is a game, information about the game.
     *
     * [More about games »](https://core.telegram.org/bots/api#games)
     * 
     * API_REMOVED: Unused in this project [SubCategories.Game].
     * FieldType: Game
     * FieldName: game
     */

    /**
     * @brief Optional. Message is a scheduled giveaway, information about the
     * giveaway
     * 
     * API_REMOVED: Unused in this project [SubCategories.Giveaway].
     * FieldType: Giveaway
     * FieldName: giveaway
     */

    /**
     * @brief Optional. A giveaway with public winners was completed
     * 
     * API_REMOVED: Unused in this project [SubCategories.Giveaway].
     * FieldType: GiveawayWinners
     * FieldName: giveawayWinners
     */

    /**
     * @brief Optional. Message is an invoice for a
     * [payment](https://core.telegram.org/bots/api#payments), information about
     * the invoice.
     *
     * [More about payments »](https://core.telegram.org/bots/api#payments)
     * 
     * API_REMOVED: Unused in this project [SubCategories.Payment].
     * FieldType: Invoice
     * FieldName: invoice
     */

    /**
     * @brief Optional. Message is a shared location, information about the
     * location
     * 
     * API_REMOVED: Unused in this project [SubCategories.Location].
     * FieldType: Location
     * FieldName: location
     */

    /**
     * @brief Optional. Message is a native poll, information about the poll
     */
    std::optional<Poll> poll;

    /**
     * @brief Optional. Message is a venue, information about the venue
     * 
     * API_REMOVED: Unused in this project [SubCategories.Location].
     * FieldType: Venue
     * FieldName: venue
     */
};

}  // namespace api::types