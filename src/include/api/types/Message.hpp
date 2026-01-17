#pragma once

#include <api/types/Animation.hpp>
#include <api/types/Chat.hpp>
#include <api/types/ChatBoostAdded.hpp>
#include <api/types/ChatPermissions.hpp>
#include <api/types/ChatPhoto.hpp>
#include <api/types/Dice.hpp>
#include <api/types/Document.hpp>
#include <api/types/ExternalReplyInfo.hpp>
#include <api/types/ForumTopicClosed.hpp>
#include <api/types/ForumTopicCreated.hpp>
#include <api/types/ForumTopicEdited.hpp>
#include <api/types/ForumTopicReopened.hpp>
#include <api/types/InlineKeyboardMarkup.hpp>
#include <api/types/GeneralForumTopicHidden.hpp>
#include <api/types/GeneralForumTopicUnhidden.hpp>
#include <api/types/LinkPreviewOptions.hpp>
#include <api/types/User.hpp>
#include <api/types/MessageAutoDeleteTimerChanged.hpp>
#include <api/types/MessageOrigins.hpp>
#include <api/types/MessageEntity.hpp>
#include <api/types/PhotoSize.hpp>
#include <api/types/Poll.hpp>
#include <api/types/TextQuote.hpp>
#include <api/types/Story.hpp>
#include <api/types/Sticker.hpp>
#include <api/types/Video.hpp>
#include <api/types/VideoChatStarted.hpp>
#include <api/types/VideoChatEnded.hpp>
#include <api/types/VideoChatParticipantsInvited.hpp>
#include <api/types/VideoChatScheduled.hpp>
#include <api/internal/RecursiveOptional.hpp>

#include <cstdint>
#include <optional>

namespace api::types {

/**
 * @brief This object represents a message.
 */
struct Message {
    using messageId_type = std::int32_t;
    using messageThreadId_type = std::int32_t;

    /**
     * @brief Unique message identifier inside this chat
     */
    messageId_type messageId;

    /**
     * @brief Optional. Unique identifier of a message thread to which the
     * message belongs; for supergroups only
     */
    std::optional<messageThreadId_type> messageThreadId;

    /**
     * @brief Optional. Sender of the message; empty for messages sent to
     * channels.
     *
     * For backward compatibility, the field contains a fake sender user in
     * non-channel chats, if the message was sent on behalf of a chat.
     */
    std::optional<User> from;

    /**
     * @brief Optional. Sender of the message, sent on behalf of a chat.
     *
     * For example, the channel itself for channel posts, the supergroup itself
     * for messages from anonymous group administrators, the linked channel for
     * messages automatically forwarded to the discussion group. For backward
     * compatibility, the field from contains a fake sender user in non-channel
     * chats, if the message was sent on behalf of a chat.
     */
    std::optional<Chat> senderChat;

    /**
     * @brief Optional. If the sender of the message boosted the chat, the
     * number of boosts added by the user
     */
    std::optional<std::int32_t> senderBoostCount;

    /**
     * @brief Optional. The bot that actually sent the message on behalf of the
     * business account.
     *
     * Available only for outgoing messages sent on behalf of the connected
     * business account.
     */
    std::optional<User> senderBusinessBot;

    /**
     * @brief Date the message was sent in Unix time.
     *
     * It is always a positive number, representing a valid date.
     */
    std::uint32_t date;

    /**
     * @brief Optional. Unique identifier of the business connection from which
     * the message was received.
     *
     * If non-empty, the message belongs to a chat of the corresponding business
     * account that is independent from any potential bot chat which might share
     * the same identifier.
     */
    std::optional<std::string> businessConnectionId;

    /**
     * @brief Chat the message belongs to
     */
    Chat chat;

    /**
     * @brief Optional. Information about the original message for forwarded
     * messages
     */
    std::optional<MessageOrigin> forwardOrigin;

    /**
     * @brief Optional. True, if the message is sent to a forum topic
     */
    std::optional<bool> isTopicMessage;

    /**
     * @brief Optional. True, if the message is a channel post that was
     * automatically forwarded to the connected discussion group
     */
    std::optional<bool> isAutomaticForward;

    /**
     * @brief Optional. For replies in the same chat and message thread, the
     * original message.
     *
     * Note that the Message object in this field will not contain further
     * replyToMessage fields even if it itself is a reply.
     */
    api::internal::RecursiveOptional<Message> replyToMessage;

    /**
     * @brief Optional. Information about the message that is being replied to,
     * which may come from another chat or forum topic
     */
    std::optional<ExternalReplyInfo> externalReply;

    /**
     * @brief Optional. For replies that quote part of the original message, the
     * quoted part of the message
     */
    std::optional<TextQuote> quote;

    /**
     * @brief Optional. For replies to a story, the original story
     */
    std::optional<Story> replyToStory;

    /**
     * @brief Optional. Bot through which the message was sent
     */
    std::optional<User> viaBot;

    /**
     * @brief Optional. Date the message was last edited in Unix time
     */
    std::optional<std::uint32_t> editDate;

    /**
     * @brief Optional. True, if the message can't be forwarded
     */
    std::optional<bool> hasProtectedContent;

    /**
     * @brief Optional. True, if the message was sent by an implicit action, for
     * example, as an away or a greeting business message, or as a scheduled
     * message
     */
    std::optional<bool> isFromOffline;

    /**
     * @brief Optional. The unique identifier of a media message group this
     * message belongs to
     */
    std::optional<std::string> mediaGroupId;

    /**
     * @brief Optional. Signature of the post author for messages in channels,
     * or the custom title of an anonymous group administrator
     */
    std::optional<std::string> authorSignature;

    /**
     * @brief Optional. For text messages, the actual UTF-8 text of the message
     */
    std::optional<std::string> text;

    /**
     * @brief Optional. For text messages, special entities like usernames,
     * URLs, bot commands, etc. that appear in the text
     */
    std::optional<std::vector<MessageEntity>> entities;

    /**
     * @brief Optional. Options used for link preview generation for the
     * message, if it is a text message and link preview options were changed
     */
    std::optional<LinkPreviewOptions> linkPreviewOptions;

    /**
     * @brief Optional. Message is an animation, information about the
     * animation.
     *
     * For backward compatibility, when this field is set, the document field
     * will also be set
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
     * @brief Optional. Caption for the animation, audio, document, photo, video
     * or voice
     */
    std::optional<std::string> caption;

    /**
     * @brief Optional. For messages with a caption, special entities like
     * usernames, URLs, bot commands, etc. that appear in the caption
     */
    std::vector<MessageEntity> captionEntities;

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
     * FleidType: Contact
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
     * @brief Optional. Message is a native poll, information about the poll
     */
    std::optional<Poll> poll;

    /**
     * @brief Optional. Message is a venue, information about the venue.
     *
     * For backward compatibility, when this field is set, the location field
     * will also be set
     * 
     * API_REMOVED: Unused in this project [SubCategories.Location].
     * FieldType: Venue
     * FieldName: venue
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
     * @brief Optional. New members that were added to the group or supergroup
     * and information about them (the bot itself may be one of these members)
     */
    std::optional<std::vector<User>> newChatMembers;

    /**
     * @brief Optional. A member was removed from the group, information about
     * them (this member may be the bot itself)
     */
    std::optional<User> leftChatMember;

    /**
     * @brief Optional. A chat title was changed to this value
     */
    std::optional<std::string> newChatTitle;

    /**
     * @brief Optional. A chat photo was change to this value
     */
    std::optional<std::vector<PhotoSize>> newChatPhoto;

    /**
     * @brief Optional. Service message: the chat photo was deleted
     */
    std::optional<bool> deleteChatPhoto;

    /**
     * @brief Optional. Service message: the group has been created
     */
    std::optional<bool> groupChatCreated;

    /**
     * @brief Optional. Service message: the supergroup has been created.
     *
     * This field can't be received in a message coming through updates, because
     * bot can't be a member of a supergroup when it is created. It can only be
     * found in replyToMessage if someone replies to a very first message in a
     * directly created supergroup.
     */
    std::optional<bool> supergroupChatCreated;

    /**
     * @brief Optional. Service message: the channel has been created.
     *
     * This field can't be received in a message coming through updates, because
     * bot can't be a member of a channel when it is created. It can only be
     * found in replyToMessage if someone replies to a very first message in a
     * channel.
     */
    std::optional<bool> channelChatCreated;

    /**
     * @brief Optional. Service message: auto-delete timer settings changed in
     * the chat
     */
    std::optional<MessageAutoDeleteTimerChanged> messageAutoDeleteTimerChanged;

    /**
     * @brief Optional. The group has been migrated to a supergroup with the
     * specified identifier.
     *
     * This number may have more than 32 significant bits and some programming
     * languages may have difficulty/silent defects in interpreting it. But it
     * has at most 52 significant bits, so a signed 64-bit integer or
     * double-precision float type are safe for storing this identifier.
     */
    std::optional<std::int64_t> migrateToChatId;

    /**
     * @brief Optional. The supergroup has been migrated from a group with the
     * specified identifier.
     *
     * This number may have more than 32 significant bits and some programming
     * languages may have difficulty/silent defects in interpreting it. But it
     * has at most 52 significant bits, so a signed 64-bit integer or
     * double-precision float type are safe for storing this identifier.
     */
    std::optional<std::int64_t> migrateFromChatId;

    /**
     * @brief Optional. Specified message was pinned.
     *
     * Note that the Message object in this field will not contain further
     * replyToMessage fields even if it itself is a reply.
     */
    api::internal::RecursiveOptional<Message> pinnedMessage;

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
     * @brief Optional. Message is a service message about a successful payment,
     * information about the payment.
     *
     * [More about payments »](https://core.telegram.org/bots/api#payments)
     * 
     * API_REMOVED: Unused in this project [SubCategories.Payment].
     * FieldType: SuccessfulPayment
     * FieldName: successfulPayment
     */

    /**
     * @brief Optional. Service message: users were shared with the bot
     * 
     * API_REMOVED: Unused in this project [SubCategories.UserSharing].
     * FieldType: UsersShared
     * FieldName: usersShared
     */

    /**
     * @brief Optional. Service message: a chat was shared with the bot
     * 
     * API_REMOVED: Unused in this project [SubCategories.ChatSharing].
     * FieldType: ChatShared
     * FieldName: chatShared
     */

    /**
     * @brief Optional. The domain name of the website on which the user has
     * logged in.
     *
     * [More about Telegram Login »](https://core.telegram.org/widgets/login)
     */
    std::optional<std::string> connectedWebsite;

    /**
     * @brief Optional. Service message: the user allowed the bot to write
     * messages after adding it to the attachment or side menu, launching a Web
     * App from a link, or accepting an explicit request from a Web App sent by
     * the method
     * [requestWriteAccess](https://core.telegram.org/bots/webapps#initializing-mini-apps)
     * 
     * API_REMOVED: Unused in this project [SubCategories.WebApp].
     * FieldType: WriteAccessAllowed
     * FieldName: writeAccessAllowed
     */

    /**
     * @brief Optional. Telegram Passport data
     * 
     * API_REMOVED: Unused in this project [SubCategories.Passport].
     * FieldType: PassportData
     * FieldName: passportData
     */

    /**
     * @brief Optional. Service message.
     *
     * A user in the chat triggered another user's proximity alert while sharing
     * Live Location.
     * 
     * API_REMOVED: Unused in this project [SubCategories.Location].
     * FieldType: ProximityAlertTriggered
     * FieldName: proximityAlertTriggered
     */

    /**
     * @brief Optional. Service message: user boosted the chat
     */
    std::optional<ChatBoostAdded> boostAdded;

    /**
     * @brief Optional. Service message: forum topic created
     */
    std::optional<ForumTopicCreated> forumTopicCreated;

    /**
     * @brief Optional. Service message: forum topic edited
     */
    std::optional<ForumTopicEdited> forumTopicEdited;

    /**
     * @brief Optional. Service message: forum topic closed
     */
    std::optional<ForumTopicClosed> forumTopicClosed;

    /**
     * @brief Optional. Service message: forum topic reopened
     */
    std::optional<ForumTopicReopened> forumTopicReopened;

    /**
     * @brief Optional. Service message: the 'General' forum topic hidden
     */
    std::optional<GeneralForumTopicHidden> generalForumTopicHidden;

    /**
     * @brief Optional. Service message: the 'General' forum topic unhidden
     */
    std::optional<GeneralForumTopicUnhidden> generalForumTopicUnhidden;

    /**
     * @brief Optional. Service message: a scheduled giveaway was created
     * 
     * API_REMOVED: Unused in this project [SubCategories.Giveaway].
     * FieldType: GiveawayCreated
     * FieldName: giveawayCreated
     */

    /**
     * @brief Optional. The message is a scheduled giveaway message
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
     * @brief Optional. Service message: a giveaway without public winners was
     * completed
     * 
     * API_REMOVED: Unused in this project [SubCategories.Giveaway].
     * FieldType: GiveawayCompleted
     * FieldName: giveawayCompleted
     */

    /**
     * @brief Optional. Service message: video chat scheduled
     */
    std::optional<VideoChatScheduled> videoChatScheduled;

    /**
     * @brief Optional. Service message: video chat started
     */
    std::optional<VideoChatStarted> videoChatStarted;

    /**
     * @brief Optional. Service message: video chat ended
     */
    std::optional<VideoChatEnded> videoChatEnded;

    /**
     * @brief Optional. Service message: new participants invited to a video
     * chat
     */
    std::optional<VideoChatParticipantsInvited> videoChatParticipantsInvited;

    /**
     * @brief Optional. Service message: data sent by a Web App
     * 
     * API_REMOVED: Unused in this project [SubCategories.WebApp].
     * FieldType: WebAppData
     * FieldName: webAppData
     */

    /**
     * @brief Optional. Inline keyboard attached to the message.
     *
     * loginUrl buttons are represented as ordinary url buttons.
     */
    std::optional<InlineKeyboardMarkup> replyMarkup;
};

}  // namespace api::types