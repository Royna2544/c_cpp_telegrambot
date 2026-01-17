#pragma once

#include <api/types/GenericReply.hpp>
#include <api/types/InlineQueryResults.hpp>
#include <api/types/InputFile.hpp>
#include <api/types/ParsedMessage.hpp>
#include <api/types/StickerSet.hpp>
#include <api/types/fwd.hpp>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <trivial_helpers/fruit_inject.hpp>
#include <utility>
#include <variant>

#include "Utils.hpp"

/**
 * @brief Base interface for Telegram Bot API operations.
 *
 * This class provides a unified interface for interacting with the Telegram
 * Bot API. It defines methods for sending messages, media, managing chat
 * members, and handling various bot-related operations.
 */
class TgBotApi {
   public:
    using Ptr = std::add_pointer_t<TgBotApi>;
    using CPtr = std::add_pointer_t<std::add_const_t<TgBotApi>>;

    TgBotApi() = default;
    virtual ~TgBotApi() = default;

    // Disable copy and move constructors
    TgBotApi(const TgBotApi&) = delete;
    TgBotApi(TgBotApi&&) = delete;
    TgBotApi& operator=(const TgBotApi&) = delete;
    TgBotApi& operator=(TgBotApi&&) = delete;

    using FileOrString = std::variant<api::types::InputFile, std::string>;
    using FileOrMedia = std::variant<api::types::InputFile, MediaIds>;
    using StringOrMessage =
        std::variant<std::string, std::optional<api::types::Message>>;

    /**
     * @brief Describes a Parse Mode type for message formatting.
     */
    enum class ParseMode : std::uint8_t {
        None,
        Markdown,
        HTML,
        MarkdownV2,
    };

    /**
     * @brief Describes a Sticker Format type.
     */
    enum class StickerFormat : std::uint8_t { Static, Animated, Video };

    /**
     * @brief Describes a Sticker Type.
     */
    enum class StickerType : std::uint8_t { Regular, Mask, CustomEmoji };

   protected:
    // Methods to be implemented

    /**
     * @brief Sends a text message to the specified chat.
     *
     * This function sends a text message to the chat with the given `chatId`.
     * The message content is specified by the `text` parameter.
     *
     * @param chatId The ID of the chat to which the message will be sent.
     * @param text The content of the message to be sent.
     * @param replyParameters (Optional) Reply parameters for the message.
     * @param replyMarkup (Optional) An inline keyboard markup or generic reply
     * markup to be added to the message.
     * @param parseMode (Optional) The parse mode for the text. Default is None.
     *
     * @return An optional Message object representing the sent message.
     */
    virtual std::optional<api::types::Message> sendMessage_impl(
        api::types::Chat::id_type chatId, const std::string_view text,
        std::optional<api::types::ReplyParameters> replyParameters =
            std::nullopt,
        std::optional<api::types::GenericReply> replyMarkup = std::nullopt,
        const ParseMode parseMode = {}) const = 0;

    /**
     * @brief Sends an animation (GIF or video without sound) to the specified
     * chat.
     *
     * This function sends an animation to the chat with the given `chatId`.
     * The animation content is specified by the `animation` parameter.
     *
     * @param chatId The ID of the chat to which the animation will be sent.
     * @param animation The content of the animation to be sent (file or file
     * ID).
     * @param caption (Optional) The caption for the animation.
     * @param replyParameters (Optional) Reply parameters for the message.
     * @param replyMarkup (Optional) An inline keyboard markup or generic reply
     * markup to be added to the message.
     * @param parseMode (Optional) The parse mode for the caption. Default is
     * None.
     *
     * @return An optional Message object representing the sent animation
     * message.
     */
    virtual std::optional<api::types::Message> sendAnimation_impl(
        api::types::Chat::id_type chatId, FileOrString animation,
        const std::string_view caption = {},
        std::optional<api::types::ReplyParameters> replyParameters =
            std::nullopt,
        std::optional<api::types::GenericReply> replyMarkup = std::nullopt,
        const ParseMode parseMode = {}) const = 0;

    /**
     * @brief Sends a sticker to the specified chat.
     *
     * This function sends a sticker to the chat with the given `chatId`.
     * The sticker content is specified by the `sticker` parameter.
     *
     * @param chatId The ID of the chat to which the sticker will be sent.
     * @param sticker The content of the sticker to be sent (file or file ID).
     * @param replyParameters (Optional) Reply parameters for the message.
     *
     * @return An optional Message object representing the sent sticker message.
     */
    virtual std::optional<api::types::Message> sendSticker_impl(
        api::types::Chat::id_type chatId, FileOrString sticker,
        std::optional<api::types::ReplyParameters> replyParameters =
            std::nullopt) const = 0;

    /**
     * @brief Creates a new sticker set.
     *
     * This function creates a new sticker set with the given parameters.
     *
     * @param userId The ID of the user who will own the sticker set.
     * @param name The name of the sticker set.
     * @param title The title of the sticker set.
     * @param stickers The vector of InputSticker objects representing the
     * stickers in the set.
     * @param stickerType The type of the stickers in the set.
     *
     * @return True if the sticker set was created successfully, false
     * otherwise.
     */
    virtual bool createNewStickerSet_impl(
        std::int64_t userId, const std::string_view name,
        const std::string_view title,
        const std::vector<api::types::InputSticker>& stickers,
        StickerType stickerType) const = 0;

    /**
     * @brief Uploads a sticker file.
     *
     * This function uploads a sticker file for later use in sticker sets.
     *
     * @param userId The ID of the user who will own the sticker file.
     * @param sticker The sticker file to be uploaded.
     * @param stickerFormat The format of the sticker file.
     *
     * @return An optional File object representing the uploaded sticker file.
     */
    virtual std::optional<api::types::File> uploadStickerFile_impl(
        std::int64_t userId, api::types::InputFile sticker,
        const StickerFormat stickerFormat) const = 0;

    /**
     * @brief Edits a sent message's text and markup.
     *
     * This function edits a previously sent message with new text and inline
     * keyboard markup.
     *
     * @param message The message to be edited.
     * @param newText The new text for the message.
     * @param markup The new inline keyboard markup for the message.
     * @param parseMode (Optional) The parse mode for the new text. Default is
     * None.
     *
     * @return An optional Message object representing the edited message.
     */
    virtual std::optional<api::types::Message> editMessage_impl(
        const std::optional<api::types::Message>& message,
        const std::string_view newText,
        const std::optional<api::types::InlineKeyboardMarkup> markup,
        const ParseMode parseMode) const = 0;

    /**
     * @brief Edits a sent message's inline keyboard markup only.
     *
     * This function edits the inline keyboard of a previously sent message
     * without changing the text.
     *
     * @param message The message to be edited (inline message ID or Message
     * object).
     * @param markup The new inline keyboard markup for the message.
     *
     * @return An optional Message object representing the edited message.
     */
    virtual std::optional<api::types::Message> editMessageMarkup_impl(
        const StringOrMessage& message,
        const api::types::GenericReply& markup) const = 0;

    /**
     * @brief Copies a message from one chat to the same or another chat.
     *
     * This function copies a message identified by `messageId` from the chat
     * with `fromChatId`. The copied message can optionally be sent as a reply.
     *
     * @param fromChatId The ID of the chat from which the message will be
     * copied.
     * @param messageId The ID of the message to be copied.
     * @param replyParameters (Optional) Reply parameters for the copied
     * message.
     *
     * @return The message ID of the copied message.
     */
    virtual api::types::Message::messageId_type copyMessage_impl(
        api::types::Chat::id_type fromChatId,
        api::types::Message::messageId_type messageId,
        std::optional<api::types::ReplyParameters> replyParameters =
            std::nullopt) const = 0;

    /**
     * @brief Answers a callback query from an inline keyboard button.
     *
     * This function sends a response to a callback query triggered by an
     * inline keyboard button press.
     *
     * @param callbackQueryId The ID of the callback query to be answered.
     * @param text (Optional) The text to be sent in the callback query answer.
     * @param showAlert (Optional) Whether to show an alert to the user. Default
     * is false.
     *
     * @return True if the callback query was answered successfully, false
     * otherwise.
     */
    virtual bool answerCallbackQuery_impl(
        const std::string_view callbackQueryId,
        const std::string_view text = {}, bool showAlert = false) const = 0;

    /**
     * @brief Deletes a sent message.
     *
     * This function deletes a previously sent message from the chat.
     *
     * @param message The message to be deleted.
     */
    virtual void deleteMessage_impl(
        const std::optional<api::types::Message>& message) const = 0;

    /**
     * @brief Deletes multiple messages in a chat.
     *
     * This function deletes multiple messages in bulk from the specified chat.
     *
     * @param chatId The ID of the chat from which the messages will be deleted.
     * @param messageIds The vector of message IDs to be deleted.
     */
    virtual void deleteMessages_impl(
        api::types::Chat::id_type chatId,
        const std::vector<api::types::Message::messageId_type>& messageIds)
        const = 0;

    /**
     * @brief Restricts a chat member's permissions.
     *
     * This function restricts or mutes a chat member by setting new
     * permissions.
     *
     * @param chatId The ID of the chat.
     * @param userId The ID of the user to be restricted.
     * @param permissions The new permissions for the restricted user.
     * @param untilDate (Optional) The timestamp until the user will be
     * restricted. If not specified, the restriction is permanent.
     */
    virtual void restrictChatMember_impl(
        api::types::Chat::id_type chatId, api::types::User::id_type userId,
        api::types::ChatPermissions permissions,
        std::chrono::system_clock::time_point untilDate = {}) const = 0;

    /**
     * @brief Sends a document file to the specified chat.
     *
     * This function sends a document file to the chat with the given
     * parameters.
     *
     * @param chatId The ID of the chat to which the document will be sent.
     * @param document The content of the document to be sent (file or file ID).
     * @param caption (Optional) The caption for the document.
     * @param replyParameters (Optional) Reply parameters for the message.
     * @param replyMarkup (Optional) An inline keyboard markup or generic reply
     * markup to be added to the message.
     * @param parseMode (Optional) The parse mode for the caption. Default is
     * None.
     *
     * @return An optional Message object representing the sent document
     * message.
     */
    virtual std::optional<api::types::Message> sendDocument_impl(
        api::types::Chat::id_type chatId, FileOrString document,
        const std::string_view caption = {},
        std::optional<api::types::ReplyParameters> replyParameters =
            std::nullopt,
        std::optional<api::types::GenericReply> replyMarkup = std::nullopt,
        const ParseMode parseMode = {}) const = 0;

    /**
     * @brief Sends a photo to the specified chat.
     *
     * This function sends a photo to the chat with the given parameters.
     *
     * @param chatId The ID of the chat to which the photo will be sent.
     * @param photo The content of the photo to be sent (file or file ID).
     * @param caption (Optional) The caption for the photo.
     * @param replyParameters (Optional) Reply parameters for the message.
     * @param replyMarkup (Optional) An inline keyboard markup or generic reply
     * markup to be added to the message.
     * @param parseMode (Optional) The parse mode for the caption. Default is
     * None.
     *
     * @return An optional Message object representing the sent photo message.
     */
    virtual std::optional<api::types::Message> sendPhoto_impl(
        api::types::Chat::id_type chatId, FileOrString photo,
        const std::string_view caption = {},
        std::optional<api::types::ReplyParameters> replyParameters =
            std::nullopt,
        std::optional<api::types::GenericReply> replyMarkup = std::nullopt,
        const ParseMode parseMode = {}) const = 0;

    /**
     * @brief Sends a video to the specified chat.
     *
     * This function sends a video to the chat with the given parameters.
     *
     * @param chatId The ID of the chat to which the video will be sent.
     * @param video The content of the video to be sent (file or file ID).
     * @param caption (Optional) The caption for the video.
     * @param replyParameters (Optional) Reply parameters for the message.
     * @param replyMarkup (Optional) An inline keyboard markup or generic reply
     * markup to be added to the message.
     * @param parseMode (Optional) The parse mode for the caption. Default is
     * None.
     *
     * @return An optional Message object representing the sent video message.
     */
    virtual std::optional<api::types::Message> sendVideo_impl(
        api::types::Chat::id_type chatId, FileOrString video,
        const std::string_view caption = {},
        std::optional<api::types::ReplyParameters> replyParameters =
            std::nullopt,
        std::optional<api::types::GenericReply> replyMarkup = std::nullopt,
        const ParseMode parseMode = {}) const = 0;

    /**
     * @brief Sends a dice emoji to the specified chat.
     *
     * This function sends an animated emoji that displays a random value
     * (dice, darts, basketball, etc.).
     *
     * @param chatId The ID of the chat to which the dice will be sent.
     *
     * @return An optional Message object representing the sent dice message.
     */
    virtual std::optional<api::types::Message> sendDice_impl(
        api::types::Chat::id_type chatId) const = 0;

    /**
     * @brief Retrieves information about a sticker set.
     *
     * This function gets a sticker set by its name.
     *
     * @param setName The name of the sticker set to be retrieved.
     *
     * @return A StickerSet object containing information about the sticker set.
     */
    virtual api::types::StickerSet getStickerSet_impl(
        const std::string_view setName) const = 0;

    /**
     * @brief Downloads a file from Telegram servers.
     *
     * This function downloads a file identified by its file ID and saves it
     * to the specified destination.
     *
     * @param destFilename The destination filesystem path for the downloaded
     * file.
     * @param fileId The Telegram file ID of the file to be downloaded.
     *
     * @return True if the file was downloaded successfully, false otherwise.
     */
    virtual bool downloadFile_impl(const std::filesystem::path& destFilename,
                                   const std::string_view fileId) const = 0;

    /**
     * @brief Retrieves information about the bot user.
     *
     * This function gets the bot's own user information.
     *
     * @return An optional User object representing the bot user.
     */
    virtual std::optional<api::types::User> getBotUser_impl() const = 0;

    /**
     * @brief Pins a message in a chat.
     *
     * This function pins a message in the specified chat, making it visible
     * at the top of the chat.
     *
     * @param message The message to be pinned.
     *
     * @return True if the message was pinned successfully, false otherwise.
     */
    virtual bool pinMessage_impl(
        std::optional<api::types::Message> message) const = 0;

    /**
     * @brief Unpins a message in a chat.
     *
     * This function removes the pinned status from a previously pinned message.
     *
     * @param message The message to be unpinned.
     *
     * @return True if the message was unpinned successfully, false otherwise.
     */
    virtual bool unpinMessage_impl(
        std::optional<api::types::Message> message) const = 0;

    /**
     * @brief Bans a chat member from a chat.
     *
     * This function bans a user from a chat, preventing them from rejoining.
     *
     * @param chat The chat from which the user will be banned.
     * @param user The user to be banned.
     *
     * @return True if the chat member was banned successfully, false otherwise.
     */
    virtual bool banChatMember_impl(
        const api::types::Chat& chat,
        const std::optional<api::types::User>& user) const = 0;

    /**
     * @brief Unbans a previously banned chat member.
     *
     * This function removes the ban from a user, allowing them to rejoin the
     * chat.
     *
     * @param chat The chat from which the user will be unbanned.
     * @param user The user to be unbanned.
     *
     * @return True if the chat member was unbanned successfully, false
     * otherwise.
     */
    virtual bool unbanChatMember_impl(
        const api::types::Chat& chat,
        const std::optional<api::types::User>& user) const = 0;

    /**
     * @brief Retrieves information about a chat member.
     *
     * This function gets information about a specific user in a chat.
     *
     * @param chat The chat ID.
     * @param userId The user ID.
     *
     * @return An optional User object representing the chat member.
     */
    virtual std::optional<api::types::User> getChatMember_impl(
        const api::types::Chat::id_type chat,
        const api::types::User::id_type userId) const = 0;

    /**
     * @brief Sets the bot's description texts.
     *
     * This function sets both the long and short descriptions for the bot.
     *
     * @param description The long description of the bot, which may contain
     * markdown or HTML formatting.
     * @param shortDescription A short description of the bot, typically shown
     * in bot lists or previews.
     *
     * @note The descriptions provide context or information about the bot to
     * users.
     */
    virtual void setDescriptions_impl(
        const std::optional<std::string_view> description,
        const std::optional<std::string_view> shortDescription) const = 0;

    /**
     * @brief Sets reactions to a message in a chat.
     *
     * This function adds one or more reactions (emoji) to a specific message.
     *
     * @param chatId The unique identifier of the chat where the message
     * resides.
     * @param messageId The unique identifier of the message to which reactions
     * are being added.
     * @param reaction A vector of reaction types (e.g., emoji or custom
     * reactions) to apply to the message.
     * @param isBig Whether the reaction should be displayed in a larger size.
     *
     * @return True if the reactions were successfully set, false otherwise.
     *
     * @note Reactions are visual indicators for users to show their response
     * to a message.
     */
    virtual bool setMessageReaction_impl(
        api::types::Chat::id_type chatId,
        api::types::Message::messageId_type messageId,
        const std::vector<api::types::ReactionType>& reaction,
        bool isBig) const = 0;

    /**
     * @brief Use this method to set a custom title for an administrator in a
     * supergroup promoted by the bot.
     *
     * @param chatId Unique identifier for the target chat or username of the
     * target supergroup (in the format @supergroupusername)
     * @param userId Unique identifier of the target user
     * @param customTitle New custom title for the administrator; 0-16
     * characters, emoji are not allowed
     *
     * @return Returns True on success.
     */
    virtual bool setChatAdministratorCustomTitle(
        api::types::Chat::id_type chatId, api::types::User::id_type userId,
        const std::string_view customTitle) const = 0;

    /**
     * @brief Sets the list of the bot's commands.
     *
     * @param commands A vector of BotCommand objects representing the bot's
     * commands.
     * @return True if the commands were successfully set, false otherwise.
     */
    virtual bool setMyCommands_impl(
        const std::vector<api::types::BotCommand>& commands) const = 0;

    /**
     * @brief Retrieves information about a chat.
     *
     * @param chatId The unique identifier of the chat to retrieve.
     *
     * @return An optional Chat object representing the chat information.
     */
    virtual std::optional<api::types::Chat> getChat_impl(
        api::types::Chat::id_type chatId) const = 0;

    /**
     * @brief Converts FileOrMedia variant to FileOrString variant.
     *
     * This static helper function converts media identifiers to a format
     * suitable for file operations.
     *
     * @param media The media variant to convert.
     *
     * @return A FileOrString variant containing either an InputFile or file ID
     * string.
     */
    static FileOrString ToFileOrString(FileOrMedia media) {
        return std::visit(
            [](auto& x) -> FileOrString {
                using T = std::decay_t<decltype(x)>;
                if constexpr (std::is_same_v<T, api::types::InputFile>) {
                    return x;
                } else if constexpr (std::is_same_v<T, MediaIds>) {
                    return x.id;
                }
            },
            media);
    }

   public:
    /**
     * @brief Converts a ParseMode enum to its string representation.
     *
     * This compile-time function converts a ParseMode value to the
     * corresponding string used by the Telegram Bot API.
     *
     * @tparam mode The ParseMode to convert.
     *
     * @return A string_view containing the parse mode string.
     */
    template <ParseMode mode>
    static consteval std::string_view parseModeToStr() {
        switch (mode) {
            case ParseMode::Markdown:
                return "Markdown";
            case ParseMode::HTML:
                return "HTML";
            case ParseMode::MarkdownV2:
                return "MarkdownV2";
            case ParseMode::None:
                return {};
        }
    }

    /**
     * @brief Sends a reply message to a specific message.
     *
     * This overload uses the message object to extract chat and thread
     * information before sending the reply.
     *
     * @tparam mode The parse mode for the message text. Default is None.
     * @param replyToMessage The message to which the reply will be sent.
     * @param message The text content of the reply message.
     * @param replyMarkup (Optional) Reply markup to attach to the message.
     *
     * @return An optional Message object representing the sent reply message.
     */
    template <ParseMode mode = ParseMode::None>
    std::optional<api::types::Message> sendReplyMessage(
        const api::types::Message& replyToMessage,
        const std::string_view message,
        const std::optional<api::types::GenericReply> replyMarkup =
            std::nullopt) const {
        auto tid = replyToMessage.messageThreadId;
        if (!replyToMessage.isTopicMessage.value_or(false)) {
            tid.reset();
        }
        return sendReplyMessage<mode>(replyToMessage.chat.id,
                                      replyToMessage.messageId, tid, message,
                                      replyMarkup);
    }

    /**
     * @brief Sends a reply message with explicit chat and message parameters.
     *
     * This overload allows direct specification of chat ID, message ID, and
     * thread ID for sending replies.
     *
     * @tparam mode The parse mode for the message text. Default is None.
     * @param chatId The ID of the chat where the reply will be sent.
     * @param messageId The ID of the message to reply to.
     * @param messageTid (Optional) The message thread ID for topic-based chats.
     * @param message The text content of the reply message.
     * @param replyMarkup (Optional) Reply markup to attach to the message.
     *
     * @return An optional Message object representing the sent reply message.
     */
    template <ParseMode mode = ParseMode::None>
    std::optional<api::types::Message> sendReplyMessage(
        const api::types::Chat::id_type chatId,
        const api::types::Message::messageId_type messageId,
        const std::optional<api::types::Message::messageThreadId_type>
            messageTid,
        const std::string_view message,
        std::optional<api::types::GenericReply> replyMarkup =
            std::nullopt) const {
        return sendMessage_impl(chatId, message,
                                createReplyParameters(messageId, messageTid),
                                std::move(replyMarkup), mode);
    }

    /**
     * @brief Sends a message to a specified chat.
     *
     * This function sends a text message to the chat identified by the chat ID.
     *
     * @tparam mode The parse mode for the message text. Default is None.
     * @param chatId The ID of the chat to which the message will be sent.
     * @param message The text content of the message to be sent.
     * @param markup (Optional) Reply markup to attach to the message.
     *
     * @return An optional Message object representing the sent message.
     */
    template <ParseMode mode = ParseMode::None>
    std::optional<api::types::Message> sendMessage(
        const ChatIds& chatId, const std::string_view message,
        std::optional<api::types::GenericReply> markup = std::nullopt) const {
        return sendMessage_impl(chatId, message, std::nullopt,
                                std::move(markup), mode);
    }

    /**
     * @brief Sends an animation as a reply to a message.
     *
     * This function sends an animation (GIF or video without sound) as a reply
     * to a specific message.
     *
     * @tparam mode The parse mode for the caption. Default is None.
     * @param replyToMessage The message to which the animation will be sent as
     * a reply.
     * @param mediaId The animation file or file ID to be sent.
     * @param caption (Optional) Caption for the animation.
     *
     * @return An optional Message object representing the sent animation
     * message.
     */
    template <ParseMode mode = ParseMode::None>
    std::optional<api::types::Message> sendReplyAnimation(
        const std::optional<api::types::Message>& replyToMessage,
        const FileOrMedia& mediaId, const std::string_view caption = {}) const {
        return sendAnimation_impl(
            replyToMessage->chat.id, ToFileOrString(mediaId), caption,
            createReplyParameters(replyToMessage), std::nullopt, mode);
    }

    /**
     * @brief Sends an animation to a specified chat.
     *
     * This function sends an animation (GIF or video without sound) to a chat.
     *
     * @tparam mode The parse mode for the caption. Default is None.
     * @param chatid The ID of the chat to which the animation will be sent.
     * @param mediaId The animation file or file ID to be sent.
     * @param caption (Optional) Caption for the animation.
     *
     * @return An optional Message object representing the sent animation
     * message.
     */
    template <ParseMode mode = ParseMode::None>
    std::optional<api::types::Message> sendAnimation(
        const ChatIds& chatid, const FileOrMedia& mediaId,
        const std::string_view caption = {}) const {
        return sendAnimation_impl(chatid, ToFileOrString(mediaId), caption,
                                  std::nullopt, std::nullopt, mode);
    }

    /**
     * @brief Sends a sticker as a reply to a message.
     *
     * This function sends a sticker as a reply to a specific message.
     *
     * @param replyToMessage The message to which the sticker will be sent as a
     * reply.
     * @param sticker The sticker file or file ID to be sent.
     *
     * @return An optional Message object representing the sent sticker message.
     */
    std::optional<api::types::Message> sendReplySticker(
        const std::optional<api::types::Message>& replyToMessage,
        const FileOrMedia& sticker) const {
        return sendSticker_impl(replyToMessage->chat.id,
                                ToFileOrString(sticker),
                                createReplyParameters(replyToMessage));
    }

    /**
     * @brief Sends a video as a reply to a message.
     *
     * This function sends a video as a reply to a specific message.
     *
     * @param replyToMessage The message to which the video will be sent as a
     * reply.
     * @param video The video file or file ID to be sent.
     * @param caption (Optional) Caption for the video.
     *
     * @return An optional Message object representing the sent video message.
     */
    std::optional<api::types::Message> sendReplyVideo(
        const std::optional<api::types::Message>& replyToMessage,
        const FileOrMedia& video, const std::string_view caption = {}) const {
        return sendVideo_impl(replyToMessage->chat.id, ToFileOrString(video),
                              caption, createReplyParameters(replyToMessage));
    }

    /**
     * @brief Sends a sticker to a specified chat.
     *
     * This function sends a sticker to the chat identified by the chat ID.
     *
     * @param chatId The ID of the chat to which the sticker will be sent.
     * @param mediaId The sticker file or file ID to be sent.
     *
     * @return An optional Message object representing the sent sticker message.
     */
    std::optional<api::types::Message> sendSticker(
        const ChatIds& chatId, const FileOrMedia& mediaId) const {
        return sendSticker_impl(chatId, ToFileOrString(mediaId));
    }

    /**
     * @brief Edits the text and markup of a sent message.
     *
     * This function edits both the text content and inline keyboard of a
     * previously sent message.
     *
     * @tparam mode The parse mode for the new text. Default is None.
     * @param message The message to be edited.
     * @param newText The new text content for the message.
     * @param markup (Optional) The new inline keyboard markup for the message.
     *
     * @return An optional Message object representing the edited message.
     */
    template <ParseMode mode = ParseMode::None>
    std::optional<api::types::Message> editMessage(
        const std::optional<api::types::Message>& message,
        const std::string_view newText,
        std::optional<api::types::InlineKeyboardMarkup> markup =
            std::nullopt) const {
        return editMessage_impl(message, newText, std::move(markup), mode);
    }

    /**
     * @brief Edits only the inline keyboard of a sent message.
     *
     * This function updates the inline keyboard markup of a message without
     * changing its text content.
     *
     * @param message The message to be edited (message object or inline message
     * ID).
     * @param replyMarkup The new reply markup for the message.
     *
     * @return An optional Message object representing the edited message.
     */
    std::optional<api::types::Message> editMessageMarkup(
        const StringOrMessage& message,
        const api::types::GenericReply& replyMarkup) const {
        return editMessageMarkup_impl(message, replyMarkup);
    }

    /**
     * @brief Copies a message and sends it as a reply to the same message.
     *
     * This convenience function copies a message and uses the same message as
     * the reply target.
     *
     * @param message The message to copy and reply to.
     *
     * @return The message ID of the copied message.
     */
    api::types::Message::messageId_type copyAndReplyAsMessage(
        const std::optional<api::types::Message>& message) const {
        return copyAndReplyAsMessage(message, message);
    }

    /**
     * @brief Copies a message and sends it as a reply to another message.
     *
     * This function copies the content of one message and sends it as a reply
     * to a different message.
     *
     * @param message The message to copy.
     * @param replyToMessage The message to which the copied message will be
     * sent as a reply.
     *
     * @return The message ID of the copied message.
     */
    api::types::Message::messageId_type copyAndReplyAsMessage(
        const std::optional<api::types::Message>& message,
        const std::optional<api::types::Message>& replyToMessage) const {
        return copyMessage_impl(message->chat.id, message->messageId,
                                createReplyParameters(replyToMessage));
    }

    /**
     * @brief Answers a callback query from an inline keyboard.
     *
     * This function sends a response to a callback query triggered by an
     * inline button press.
     *
     * @param callbackQueryId The unique identifier for the callback query.
     * @param text (Optional) Text to display to the user.
     * @param showAlert (Optional) Whether to show an alert dialog. Default is
     * false.
     *
     * @return True if the callback was answered successfully, false otherwise.
     */
    bool answerCallbackQuery(const std::string_view callbackQueryId,
                             const std::string_view text = {},
                             bool showAlert = false) const {
        return answerCallbackQuery_impl(callbackQueryId, text, showAlert);
    }

    /**
     * @brief Deletes a message from a chat.
     *
     * This function removes a message from the chat.
     *
     * @param message The message to delete.
     */
    void deleteMessage(
        const std::optional<api::types::Message>& message) const {
        deleteMessage_impl(message);
    }

    /**
     * @brief Deletes multiple messages from a chat.
     *
     * This function removes multiple messages in bulk.
     *
     * @param chatid The ID of the chat from which messages will be deleted.
     * @param messages Vector of message IDs to delete.
     */
    void deleteMessages(api::types::Chat::id_type chatid,
                        const std::vector<api::types::Message::messageId_type>&
                            messages) const {
        deleteMessages_impl(chatid, messages);
    }

    /**
     * @brief Mutes a chat member by restricting their permissions.
     *
     * This function restricts what a user can do in a chat for a specified
     * duration.
     *
     * @param chatId The ID of the chat.
     * @param userId The ID of the user to mute.
     * @param permissions The new permission set for the user.
     * @param untilDate The timestamp when the mute will expire.
     */
    void muteChatMember(api::types::Chat::id_type chatId,
                        api::types::User::id_type userId,
                        api::types::ChatPermissions permissions,
                        std::chrono::system_clock::time_point untilDate) const {
        restrictChatMember_impl(chatId, userId, permissions, untilDate);
    }

    /**
     * @brief Sends a document to a specified chat.
     *
     * This function sends a document file to a chat.
     *
     * @tparam mode The parse mode for the caption. Default is None.
     * @param chatId The ID of the chat to which the document will be sent.
     * @param document The document file or file ID to be sent.
     * @param caption (Optional) Caption for the document.
     * @param replyMarkup (Optional) Reply markup to attach to the message.
     *
     * @return An optional Message object representing the sent document
     * message.
     */
    template <ParseMode mode = ParseMode::None>
    std::optional<api::types::Message> sendDocument(
        ChatIds chatId, FileOrMedia document,
        const std::string_view caption = {},
        std::optional<api::types::GenericReply> replyMarkup =
            std::nullopt) const {
        return sendDocument_impl(chatId, ToFileOrString(std::move(document)),
                                 caption, std::nullopt, std::move(replyMarkup),
                                 mode);
    }

    /**
     * @brief Sends a document as a reply to a message.
     *
     * This function sends a document file as a reply to a specific message.
     *
     * @tparam mode The parse mode for the caption. Default is None.
     * @param message The message to which the document will be sent as a reply.
     * @param document The document file or file ID to be sent.
     * @param caption (Optional) Caption for the document.
     * @param replyMarkup (Optional) Reply markup to attach to the message.
     *
     * @return An optional Message object representing the sent document
     * message.
     */
    template <ParseMode mode = ParseMode::None>
    std::optional<api::types::Message> sendReplyDocument(
        std::optional<api::types::Message> message, FileOrMedia document,
        const std::string_view caption = {},
        std::optional<api::types::GenericReply> replyMarkup =
            std::nullopt) const {
        return sendDocument_impl(
            message->chat.id, ToFileOrString(std::move(document)), caption,
            createReplyParameters(message), std::move(replyMarkup), mode);
    }

    /**
     * @brief Sends a photo to a specified chat.
     *
     * This function sends a photo to a chat with optional caption and reply
     * parameters.
     *
     * @tparam mode The parse mode for the caption. Default is None.
     * @param chatId The ID of the chat to which the photo will be sent.
     * @param photo The photo file or file ID to be sent.
     * @param caption (Optional) Caption for the photo.
     * @param replyParameters (Optional) Reply parameters for the message.
     * @param replyMarkup (Optional) Reply markup to attach to the message.
     *
     * @return An optional Message object representing the sent photo message.
     */
    template <ParseMode mode = ParseMode::None>
    std::optional<api::types::Message> sendPhoto(
        ChatIds chatId, FileOrMedia photo, const std::string_view caption = {},
        std::optional<api::types::ReplyParameters> replyParameters =
            std::nullopt,
        std::optional<api::types::GenericReply> replyMarkup =
            std::nullopt) const {
        return sendPhoto_impl(chatId, ToFileOrString(std::move(photo)), caption,
                              std::move(replyParameters),
                              std::move(replyMarkup), mode);
    }

    /**
     * @brief Sends a video to a specified chat.
     *
     * This function sends a video to a chat with optional caption and reply
     * parameters.
     *
     * @tparam mode The parse mode for the caption. Default is None.
     * @param chatId The ID of the chat to which the video will be sent.
     * @param video The video file or file ID to be sent.
     * @param caption (Optional) Caption for the video.
     * @param replyParameters (Optional) Reply parameters for the message.
     * @param replyMarkup (Optional) Reply markup to attach to the message.
     *
     * @return An optional Message object representing the sent video message.
     */
    template <ParseMode mode = ParseMode::None>
    std::optional<api::types::Message> sendVideo(
        ChatIds chatId, FileOrMedia video, const std::string_view caption = {},
        std::optional<api::types::ReplyParameters> replyParameters =
            std::nullopt,
        std::optional<api::types::GenericReply> replyMarkup =
            std::nullopt) const {
        return sendVideo_impl(chatId, ToFileOrString(std::move(video)), caption,
                              std::move(replyParameters),
                              std::move(replyMarkup), mode);
    }

    /**
     * @brief Sends a photo as a reply to a message.
     *
     * This function sends a photo as a reply to a specific message.
     *
     * @tparam mode The parse mode for the caption. Default is None.
     * @param replyToMessage The message to which the photo will be sent as a
     * reply.
     * @param photo The photo file or file ID to be sent.
     * @param caption (Optional) Caption for the photo.
     *
     * @return An optional Message object representing the sent photo message.
     */
    template <ParseMode mode = ParseMode::None>
    std::optional<api::types::Message> sendReplyPhoto(
        const std::optional<api::types::Message>& replyToMessage,
        const FileOrMedia& photo, const std::string_view caption = {}) const {
        return sendPhoto<mode>(replyToMessage->chat.id, photo, caption,
                               createReplyParameters(replyToMessage));
    }

    /**
     * @brief Downloads a file from Telegram servers.
     *
     * This function downloads a file and saves it to the local filesystem.
     *
     * @param path The destination path where the file will be saved.
     * @param fileid The Telegram file ID of the file to download.
     *
     * @return True if the download was successful, false otherwise.
     */
    [[nodiscard]] bool downloadFile(const std::filesystem::path& path,
                                    const std::string_view fileid) const {
        return downloadFile_impl(path, fileid);
    }

    /**
     * @brief Retrieves the bot's user information.
     *
     * This function gets information about the bot itself.
     *
     * @return An optional User object representing the bot.
     */
    [[nodiscard]] std::optional<api::types::User> getBotUser() const {
        return getBotUser_impl();
    }

    /**
     * @brief Sends a dice emoji to a chat.
     *
     * This function sends an animated emoji showing a random dice roll.
     *
     * @param chat The ID of the chat to which the dice will be sent.
     *
     * @return An optional Message object representing the sent dice message.
     */
    std::optional<api::types::Message> sendDice(
        const api::types::Chat::id_type chat) const {
        return sendDice_impl(chat);
    }

    /**
     * @brief Retrieves a sticker set by name.
     *
     * This function gets information about a sticker set.
     *
     * @param setName The name of the sticker set.
     *
     * @return A StickerSet object containing the sticker set information.
     */
    [[nodiscard]] api::types::StickerSet getStickerSet(
        const std::string_view setName) const {
        return getStickerSet_impl(setName);
    }

    /**
     * @brief Creates a new sticker set.
     *
     * This function creates a new sticker set owned by a specific user.
     *
     * @param userId The ID of the user who will own the sticker set.
     * @param name The name identifier for the sticker set.
     * @param title The display title of the sticker set.
     * @param stickers Vector of stickers to include in the set.
     * @param stickerType The type of stickers in the set.
     *
     * @return True if the sticker set was created successfully, false
     * otherwise.
     */
    bool createNewStickerSet(
        std::int64_t userId, const std::string_view name,
        const std::string_view title,
        const std::vector<api::types::InputSticker>& stickers,
        StickerType stickerType) const {
        return createNewStickerSet_impl(userId, name, title, stickers,
                                        stickerType);
    }

    /**
     * @brief Uploads a sticker file to Telegram servers.
     *
     * This function uploads a sticker file for use in sticker sets.
     *
     * @param userId The ID of the user uploading the sticker.
     * @param sticker The sticker file to upload.
     * @param stickerFormat The format of the sticker (static, animated, or
     * video).
     *
     * @return An optional File object representing the uploaded sticker file.
     */
    [[nodiscard]] std::optional<api::types::File> uploadStickerFile(
        std::int64_t userId, api::types::InputFile sticker,
        const StickerFormat stickerFormat) const {
        return uploadStickerFile_impl(userId, std::move(sticker),
                                      stickerFormat);
    }

    /**
     * @brief Pins a message in a chat.
     *
     * This function pins a message to the top of the chat.
     *
     * @param message The message to pin.
     *
     * @return True if the message was pinned successfully, false otherwise.
     */
    bool pinMessage(std::optional<api::types::Message> message) {
        return pinMessage_impl(std::move(message));
    }

    /**
     * @brief Unpins a message in a chat.
     *
     * This function removes the pinned status from a message.
     *
     * @param message The message to unpin.
     *
     * @return True if the message was unpinned successfully, false otherwise.
     */
    bool unpinMessage(std::optional<api::types::Message> message) {
        return unpinMessage_impl(std::move(message));
    }

    /**
     * @brief Bans a user from a chat.
     *
     * This function bans a user, preventing them from accessing the chat.
     *
     * @param chat The chat from which to ban the user.
     * @param user The user to ban.
     *
     * @return True if the user was banned successfully, false otherwise.
     */
    bool banChatMember(const api::types::Chat& chat,
                       const std::optional<api::types::User>& user) {
        return banChatMember_impl(chat, user);
    }

    /**
     * @brief Unbans a user from a chat.
     *
     * This function removes a ban, allowing the user to rejoin.
     *
     * @param chat The chat from which to unban the user.
     * @param user The user to unban.
     *
     * @return True if the user was unbanned successfully, false otherwise.
     */
    bool unbanChatMember(const api::types::Chat& chat,
                         const std::optional<api::types::User>& user) {
        return unbanChatMember_impl(chat, user);
    }

    /**
     * @brief Retrieves information about a chat member.
     *
     * This function gets details about a specific user in a chat.
     *
     * @param chat The chat ID.
     * @param user The user ID.
     *
     * @return An optional User object with the chat member's information.
     */
    std::optional<api::types::User> getChatMember(
        const api::types::Chat::id_type chat,
        const api::types::User::id_type user) {
        return getChatMember_impl(chat, user);
    }

    /**
     * @brief Sets the bot's description texts.
     *
     * This function updates both the long and short descriptions for the bot.
     *
     * @param description The long description text.
     * @param shortDescription The short description text.
     */
    void setDescriptions(const std::string_view description,
                         const std::string_view shortDescription) {
        setDescriptions_impl(description, shortDescription);
    }

    /**
     * @brief Sets reactions on a message.
     *
     * This function adds emoji reactions to a message on behalf of the bot.
     *
     * @param message The message to react to.
     * @param reaction Vector of reaction types to set.
     * @param isBig Whether to show the reaction in large size.
     *
     * @return True if the reactions were set successfully, false otherwise.
     */
    bool setMessageReaction(
        const std::optional<api::types::Message>& message,
        const std::vector<api::types::ReactionType>& reaction,
        bool isBig) const {
        return setMessageReaction_impl(message->chat.id, message->messageId,
                                       reaction, isBig);
    }

    /**
     * @brief Starts the bot's polling mechanism.
     *
     * This virtual function initiates the bot's message polling loop.
     * Default implementation is empty.
     */
    virtual void startPoll() {
        // Dummy implementation
    }

    /**
     * @brief Unloads a command module.
     *
     * This virtual function unloads a dynamically loaded command module.
     *
     * @param command The name of the command to unload.
     *
     * @return True if the command was unloaded successfully, false otherwise.
     * Default implementation returns false.
     */
    virtual bool unloadCommand(const std::string& /*command*/) {
        return false;  // Dummy implementation
    }

    /**
     * @brief Reloads a command module.
     *
     * This virtual function reloads a dynamically loaded command module.
     *
     * @param command The name of the command to reload.
     *
     * @return True if the command was reloaded successfully, false otherwise.
     * Default implementation returns false.
     */
    virtual bool reloadCommand(const std::string& /*command*/) {
        return false;  // Dummy implementation
    }

    /**
     * @brief Result codes for message callback handlers.
     */
    enum class AnyMessageResult : std::uint8_t {
        Handled,     ///< Message was handled, keep calling this handler
        Deregister,  ///< Deregister this handler, stop calling it
    };

    using AnyMessageCallback = std::function<AnyMessageResult(
        TgBotApi::CPtr, const api::types::Message&)>;

    /**
     * @brief Registers a callback for all incoming messages.
     *
     * This virtual function registers a handler to be called for every incoming
     * message.
     *
     * @param callback The callback function to register.
     * Default implementation is empty.
     */
    virtual void onAnyMessage(const AnyMessageCallback& callback) {
        // Dummy implementation
    }

    using CallbackQueryCallback =
        std::function<void(TgBotApi::CPtr, const api::types::CallbackQuery&)>;

    /**
     * @brief Registers a callback query handler.
     *
     * This virtual function registers a handler for callback queries from
     * inline keyboards.
     *
     * @param message The callback data identifier to listen for.
     * @param listener The callback function to execute when matched.
     * Default implementation is empty.
     */
    virtual void onCallbackQuery(std::string message,
                                 CallbackQueryCallback listener) {
        // Dummy implementation
    }

    /**
     * @brief Configuration for inline query keyboards.
     */
    struct InlineQuery {
        std::string name;         ///< Name of the query (prefix)
        std::string description;  ///< Help text for the query
        std::string
            command;  ///< Associated command name (empty if not a command)
        bool hasMoreArguments;  ///< Whether the query expects additional
                                ///< arguments
        bool enforced;  ///< Whether restricted to whitelist users or owners

        auto operator<=>(const InlineQuery& other) const = default;
    };

    using InlineCallback =
        std::function<std::vector<api::types::InlineQueryResult>(
            const std::string_view)>;

    /**
     * @brief Adds an inline query keyboard with a static result.
     *
     * This virtual function registers an inline query handler with a fixed
     * result.
     *
     * @param query The inline query configuration.
     * @param result The static inline query result to return.
     * Default implementation is empty.
     */
    virtual void addInlineQueryKeyboard(InlineQuery query,
                                        api::types::InlineQueryResult result) {
        // Dummy implementation
    }

    /**
     * @brief Adds an inline query keyboard with a dynamic callback.
     *
     * This virtual function registers an inline query handler with a dynamic
     * callback.
     *
     * @param query The inline query configuration.
     * @param result The callback function that generates inline query results.
     * Default implementation is empty.
     */
    virtual void addInlineQueryKeyboard(InlineQuery query,
                                        InlineCallback result) {
        // Dummy implementation
    }

    /**
     * @brief Removes an inline query keyboard.
     *
     * This virtual function deregisters an inline query handler.
     *
     * @param key The name/key of the inline query to remove.
     * Default implementation is empty.
     */
    virtual void removeInlineQueryKeyboard(const std::string_view key) {
        // Dummy implementation
    }

   protected:
    /**
     * @brief Creates reply parameters from a message object.
     *
     * This helper function extracts reply parameters including thread ID
     * for topic-based chats.
     *
     * @param messageToReply The message to reply to.
     *
     * @return A ReplyParameters object configured for the message.
     */
    static api::types::ReplyParameters createReplyParameters(
        const std::optional<api::types::Message>& messageToReply) {
        api::types::Message::messageId_type messageId =
            messageToReply->messageId;
        std::optional<api::types::Message::messageThreadId_type> threadId;
        if (messageToReply->isTopicMessage.value_or(false)) {
            threadId = messageToReply->messageThreadId;
        }
        return createReplyParameters(messageId, threadId);
    }

    /**
     * @brief Creates reply parameters from explicit IDs.
     *
     * This helper function creates reply parameters with explicit message
     * and thread IDs.
     *
     * @param messageId The ID of the message to reply to.
     * @param messageThreadId (Optional) The thread ID for topic-based chats.
     *
     * @return A ReplyParameters object configured with the given IDs.
     */
    static api::types::ReplyParameters createReplyParameters(
        api::types::Message::messageId_type messageId,
        std::optional<api::types::Message::messageThreadId_type>
            messageThreadId) {
        api::types::ReplyParameters params;
        params.messageId = messageId;
        params.allowSendingWithoutReply = true;
        params.messageThreadId = messageThreadId;
        return params;
    }

    virtual void registerCommandListener(
        const std::string& command,
        std::function<void(api::types::Message message)> listener) {
        // Dummy implementation
    }
};
