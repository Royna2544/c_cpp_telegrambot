#pragma once

#include <tgbot/Api.h>
#include <tgbot/EventBroadcaster.h>
#include <tgbot/types/InlineQueryResult.h>
#include <tgbot/types/InputFile.h>
#include <tgbot/types/InputSticker.h>
#include <tgbot/types/Message.h>
#include <tgbot/types/Sticker.h>
#include <tgbot/types/StickerSet.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <trivial_helpers/fruit_inject.hpp>
#include <utility>
#include <variant>

#include "ReplyParametersExt.hpp"
#include "Utils.hpp"
#include "api/typedefs.h"

using TgBot::Chat;
using TgBot::File;
using TgBot::GenericReply;
using TgBot::InputFile;
using TgBot::InputSticker;
using TgBot::Message;
using TgBot::ReactionType;
using TgBot::Sticker;
using TgBot::StickerSet;
using TgBot::User;

// Base interface for operations involving TgBot...
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

    using FileOrString = std::variant<InputFile::Ptr, std::string>;
    using FileOrMedia = std::variant<InputFile::Ptr, MediaIds>;
    using StringOrMessage = std::variant<std::string, Message::Ptr>;

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
     * @param replyMarkup (Optional) A pointer to an InlineKeyboardMarkup object
     * that represents the inline keyboard to be added to the message.
     * @param parseMode (Optional) The parse mode for the text. Default is
     * Markdown.
     *
     * @return A shared pointer to the sent message.
     */
    virtual Message::Ptr sendMessage_impl(
        ChatId chatId, const std::string_view text,
        ReplyParametersExt::Ptr replyParameters = nullptr,
        GenericReply::Ptr replyMarkup = nullptr,
        const TgBot::Api::ParseMode parseMode = {}) const = 0;

    /**
     * @brief Sends a GIF to the specified chat.
     *
     * This function sends a GIF to the chat with the given `chatId`.
     * The GIF content is specified by the `animation` parameter.
     *
     * @param chatId The ID of the chat to which the GIF will be sent.
     * @param animation The content of the GIF to be sent.
     * @param caption (Optional) The caption for the GIF.
     * @param replyParameters (Optional) A pointer to a ReplyParametersExt
     * object that represents the reply parameters for the message.
     * @param replyMarkup (Optional) A pointer to an InlineKeyboardMarkup object
     * that represents the inline keyboard to be added to the message.
     * @param parseMode (Optional) The parse mode for the caption. Default is
     * Markdown.
     *
     * @return A shared pointer to the sent GIF message.
     */
    virtual Message::Ptr sendAnimation_impl(
        ChatId chatId, FileOrString animation,
        const std::string_view caption = {},
        ReplyParametersExt::Ptr replyParameters = nullptr,
        GenericReply::Ptr replyMarkup = nullptr,
        const TgBot::Api::ParseMode parseMode = {}) const = 0;

    /**
     * @brief Sends a sticker to the specified chat.
     *
     * This function sends a sticker to the chat with the given `chatId`.
     * The sticker content is specified by the `sticker` parameter.
     *
     * @param chatId The ID of the chat to which the sticker will be sent.
     * @param sticker The content of the sticker to be sent.
     * @param replyParameters (Optional) A pointer to a ReplyParametersExt
     * object that represents the reply parameters for the message.
     *
     * @return A shared pointer to the sent sticker message.
     */
    virtual Message::Ptr sendSticker_impl(
        ChatId chatId, FileOrString sticker,
        ReplyParametersExt::Ptr replyParameters = nullptr) const = 0;

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
     * @return A boolean value indicating whether the sticker set was created
     * successfully.
     */
    virtual bool createNewStickerSet_impl(
        std::int64_t userId, const std::string_view name,
        const std::string_view title,
        const std::vector<InputSticker::Ptr>& stickers,
        Sticker::Type stickerType) const = 0;

    /**
     * @brief Uploads a sticker file.
     *
     * This function uploads a sticker file with the given parameters.
     *
     * @param userId The ID of the user who will own the sticker file.
     * @param sticker The sticker file to be uploaded.
     * @param stickerFormat The format of the sticker file.
     *
     * @return A shared pointer to the uploaded sticker file.
     */
    virtual File::Ptr uploadStickerFile_impl(
        std::int64_t userId, InputFile::Ptr sticker,
        const TgBot::Api::StickerFormat stickerFormat) const = 0;

    /**
     * @brief Edits a sent message.
     *
     * This function edits a sent message with the given parameters.
     *
     * @param message The pointer to the message to be edited.
     * @param newText The new text for the message.
     * @param markup The new inline keyboard markup for the message.
     * @param parseMode (Optional) The parse mode for the new text. Default is
     * empty
     *
     * @return A shared pointer to the edited message.
     */
    virtual Message::Ptr editMessage_impl(
        const Message::Ptr& message, const std::string_view newText,
        const TgBot::InlineKeyboardMarkup::Ptr& markup,
        const TgBot::Api::ParseMode parseMode) const = 0;

    /**
     * @brief Edits a sent message with new markup.
     *
     * This function edits a sent message with the given parameters.
     *
     * @param message The pointer to the message to be edited.
     * @param markup The new inline keyboard markup for the message.
     *
     * @return A shared pointer to the edited message.
     */
    virtual Message::Ptr editMessageMarkup_impl(
        const StringOrMessage& message,
        const GenericReply::Ptr& markup) const = 0;

    /**
     * @brief Copies a message from one chat to another.
     *
     * This function copies a message from the chat with the given `fromChatId`
     * to the chat with the given `toChatId`.
     *
     * @param fromChatId The ID of the chat from which the message will be
     * copied.
     * @param messageId The ID of the message to be copied.
     * @param replyParameters (Optional) A pointer to a ReplyParametersExt
     * object that represents the reply parameters for the copied message.
     *
     * @return The ID of the copied message.
     */
    virtual MessageId copyMessage_impl(
        ChatId fromChatId, MessageId messageId,
        ReplyParametersExt::Ptr replyParameters = nullptr) const = 0;

    /**
     * @brief Answers a callback query.
     *
     * This function answers a callback query with the given parameters.
     *
     * @param callbackQueryId The ID of the callback query to be answered.
     * @param text (Optional) The text to be sent in the callback query answer.
     * @param showAlert (Optional) A boolean indicating whether to show an
     * alert.
     *
     * @return A boolean value indicating whether the callback query was
     * answered successfully.
     */
    virtual bool answerCallbackQuery_impl(
        const std::string_view callbackQueryId,
        const std::string_view text = {}, bool showAlert = false) const = 0;

    /**
     * @brief Deletes a sent message.
     *
     * This function deletes a sent message with the given parameters.
     *
     * @param message The pointer to the message to be deleted.
     */
    virtual void deleteMessage_impl(const Message::Ptr& message) const = 0;

    /**
     * @brief Deletes a range of sent messages.
     *
     * This function deletes a range of sent messages with the given parameters.
     *
     * @param chatId The ID of the chat from which the messages will be deleted.
     * @param messageIds The vector of IDs of the messages to be deleted.
     */
    virtual void deleteMessages_impl(
        ChatId chatId, const std::vector<MessageId>& messageIds) const = 0;

    /**
     * @brief Mutes a chat member.
     *
     * This function mutes a chat member with the given parameters.
     *
     * @param chatId The ID of the chat.
     * @param userId The ID of the user to be muted.
     * @param permissions The new permissions for the muted user.
     * @param untilDate (Optional) The timestamp until the user will be muted.
     */
    virtual void restrictChatMember_impl(
        ChatId chatId, UserId userId, TgBot::ChatPermissions::Ptr permissions,
        std::chrono::system_clock::time_point untilDate = {}) const = 0;

    /**
     * @brief Sends a file to the specified chat.
     *
     * This function sends a file to the chat with the given parameters.
     *
     * @param chatId The ID of the chat to which the file will be sent.
     * @param document The content of the file to be sent.
     * @param caption (Optional) The caption for the file.
     * @param replyParameters (Optional) A pointer to a ReplyParametersExt
     * object that represents the reply parameters for the message.
     * @param replyMarkup (Optional) A pointer to an InlineKeyboardMarkup object
     * that represents the inline keyboard to be added to the message.
     * @param parseMode (Optional) The parse mode for the caption. Default is
     * Markdown.
     *
     * @return A shared pointer to the sent file message.
     */
    virtual Message::Ptr sendDocument_impl(
        ChatId chatId, FileOrString document,
        const std::string_view caption = {},
        ReplyParametersExt::Ptr replyParameters = nullptr,
        GenericReply::Ptr replyMarkup = nullptr,
        const TgBot::Api::ParseMode parseMode = {}) const = 0;

    /**
     * @brief Sends a photo to the specified chat.
     *
     * This function sends a photo to the chat with the given parameters.
     *
     * @param chatId The ID of the chat to which the photo will be sent.
     * @param photo The content of the photo to be sent.
     * @param caption (Optional) The caption for the photo.
     * @param replyParameters (Optional) A pointer to a ReplyParametersExt
     * object that represents the reply parameters for the message.
     * @param replyMarkup (Optional) A pointer to an InlineKeyboardMarkup object
     * that represents the inline keyboard to be added to the message.
     * @param parseMode (Optional) The parse mode for the caption. Default is
     * Markdown.
     *
     * @return A shared pointer to the sent photo message.
     */
    virtual Message::Ptr sendPhoto_impl(
        ChatId chatId, FileOrString photo, const std::string_view caption = {},
        ReplyParametersExt::Ptr replyParameters = nullptr,
        GenericReply::Ptr replyMarkup = nullptr,
        const TgBot::Api::ParseMode parseMode = {}) const = 0;

    /**
     * @brief Sends a video to the specified chat.
     *
     * This function sends a video to the chat with the given parameters.
     *
     * @param chatId The ID of the chat to which the video will be sent.
     * @param video The content of the video to be sent.
     * @param caption (Optional) The caption for the video.
     * @param replyParameters (Optional) A pointer to a ReplyParametersExt
     * object that represents the reply parameters for the message.
     * @param replyMarkup (Optional) A pointer to an InlineKeyboardMarkup object
     * that represents the inline keyboard to be added to the message.
     * @param parseMode (Optional) The parse mode for the caption. Default is
     * Markdown.
     *
     * @return A shared pointer to the sent video message.
     */
    virtual Message::Ptr sendVideo_impl(
        ChatId chatId, FileOrString video, const std::string_view caption = {},
        ReplyParametersExt::Ptr replyParameters = nullptr,
        GenericReply::Ptr replyMarkup = nullptr,
        const TgBot::Api::ParseMode parseMode = {}) const = 0;

    /**
     * @brief Sends a dice to the specified chat.
     *
     * This function sends a dice to the chat with the given parameters.
     *
     * @param chatId The ID of the chat to which the dice will be sent.
     *
     * @return A shared pointer to the sent dice message.
     */
    virtual Message::Ptr sendDice_impl(ChatId chatId) const = 0;

    /**
     * @brief Gets a sticker set.
     *
     * This function gets a sticker set with the given parameters.
     *
     * @param setName The name of the sticker set to be retrieved.
     *
     * @return A shared pointer to the retrieved sticker set.
     */
    virtual StickerSet::Ptr getStickerSet_impl(
        const std::string_view setName) const = 0;

    /**
     * @brief Downloads a file from the specified file ID.
     *
     * This function downloads a file from the specified file ID.
     *
     * @param destFilename The destination filename for the downloaded file.
     * @param fileId The ID of the file to be downloaded.
     *
     * @return A boolean value indicating whether the file was downloaded
     * successfully.
     */
    virtual bool downloadFile_impl(const std::filesystem::path& destFilename,
                                   const std::string_view fileId) const = 0;

    /**
     * @brief Gets the bot user.
     *
     * This function retrieves the bot user.
     *
     * @return A shared pointer to the bot user.
     */
    virtual User::Ptr getBotUser_impl() const = 0;

    /**
     * @brief Pins a message in the specified chat.
     *
     * This function pins a message in the specified chat.
     *
     * @param message The pointer to the message to be pinned.
     *
     * @return A boolean value indicating whether the message was pinned
     * successfully.
     */
    virtual bool pinMessage_impl(Message::Ptr message) const = 0;

    /**
     * @brief Unpins a message in the specified chat.
     *
     * This function unpins a message in the specified chat.
     *
     * @param message The pointer to the message to be unpinned.
     *
     * @return A boolean value indicating whether the message was unpinned
     * successfully.
     */
    virtual bool unpinMessage_impl(Message::Ptr message) const = 0;

    /**
     * @brief Bans a chat member.
     *
     * This function bans a chat member with the given parameters.
     *
     * @param chat The pointer to the chat.
     * @param user The pointer to the user to be banned.
     *
     * @return A boolean value indicating whether the chat member was banned
     * successfully.
     */
    virtual bool banChatMember_impl(const Chat::Ptr& chat,
                                    const User::Ptr& user) const = 0;

    /**
     * @brief Unbans a chat member.
     *
     * This function unbans a chat member with the given parameters.
     *
     * @param chat The pointer to the chat.
     * @param user The pointer to the user to be unbanned.
     *
     * @return A boolean value indicating whether the chat member was unbanned
     * successfully.
     */
    virtual bool unbanChatMember_impl(const Chat::Ptr& chat,
                                      const User::Ptr& user) const = 0;

    /**
     * @brief Gets a chat member.
     *
     * This function gets a chat member with the given parameters.
     * @param chat The chat id of the chat.
     * @param userId The user id of the user.
     */
    virtual User::Ptr getChatMember_impl(const ChatId chat,
                                         const UserId userId) const = 0;
    /**
     * @brief Sets the descriptions for a chat.
     *
     * @param description The long description of the chat, which may contain
     * markdown or HTML.
     * @param shortDescription A short description of the chat, typically shown
     * in a preview.
     *
     * @note The descriptions are used to provide context or information about
     * the chat.
     */
    virtual void setDescriptions_impl(
        const std::string_view description,
        const std::string_view shortDescription) const = 0;

    /**
     * @brief Sets a reaction to a message in a chat.
     *
     * @param chatId The unique identifier of the chat where the message
     * resides.
     * @param messageId The unique identifier of the message to which the
     * reaction is being added.
     * @param reaction A vector of reaction types (e.g., emoji or custom
     * reactions) to apply to the message.
     * @param isBig A flag indicating whether the reaction should be displayed
     * in a larger size.
     *
     * @return True if the reaction was successfully set, otherwise false.
     *
     * @note Reactions are visual indicators for users to show their response to
     * a message.
     */
    virtual bool setMessageReaction_impl(
        ChatId chatId, MessageId messageId,
        const std::vector<ReactionType::Ptr>& reaction, bool isBig) const = 0;

    static FileOrString ToFileOrString(FileOrMedia media) {
        return std::visit(
            [](auto& x) -> FileOrString {
                using T = std::decay_t<decltype(x)>;
                if constexpr (std::is_same_v<T, InputFile::Ptr>) {
                    return x;
                } else if constexpr (std::is_same_v<T, MediaIds>) {
                    return x.id;
                }
            },
            media);
    }

   public:
    // Convience wrappers over real API
    using ParseMode = TgBot::Api::ParseMode;

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
     * @brief Sends a reply message to the specified message.
     *
     * This function uses the TgBot::Api::sendMessage method to send a reply
     * message to the given `replyToMessage`. The function creates a
     * `ReplyParameters` object to specify the reply message's ID and chat ID.
     *
     * @param replyToMessage The message to which the reply will be sent.
     * @param message The text content of the reply message.
     *
     * @return A shared pointer to the sent message.
     */
    template <ParseMode mode = ParseMode::None>
    Message::Ptr sendReplyMessage(
        const Message::Ptr& replyToMessage, const std::string_view message,
        const GenericReply::Ptr& replyMarkup = nullptr) const {
        auto tid = replyToMessage->messageThreadId;
        if (!replyToMessage->isTopicMessage.value_or(false)) {
            tid.reset();
        }
        return sendReplyMessage<mode>(replyToMessage->chat->id,
                                      replyToMessage->messageId, tid, message,
                                      replyMarkup);
    }

    template <ParseMode mode = ParseMode::None>
    Message::Ptr sendReplyMessage(
        const ChatId chatId, const MessageId messageId,
        const std::optional<MessageThreadId> messageTid,
        const std::string_view message,
        const GenericReply::Ptr& replyMarkup = nullptr) const {
        return sendMessage_impl(chatId, message,
                                createReplyParameters(messageId, messageTid),
                                replyMarkup, mode);
    }

    /**
     * @brief Sends a message to the specified chat reference.
     *
     * This function uses the TgBot::Api::sendMessage method to send a message
     * to the chat specified by the given `chatId`. The function takes the chat
     * ID and the text content of the message to be sent.
     *
     * @param chatId The ID of the chat to which the message will be sent.
     * @param message The text content of the message to be sent.
     *
     * @return A shared pointer to the sent message.
     */
    template <ParseMode mode = ParseMode::None>
    Message::Ptr sendMessage(const ChatIds& chatId,
                             const std::string_view message,
                             const GenericReply::Ptr& markup = nullptr) const {
        return sendMessage_impl(chatId, message, nullptr, markup, mode);
    }

    template <ParseMode mode = ParseMode::None>
    Message::Ptr sendReplyAnimation(const Message::Ptr& replyToMessage,
                                    const FileOrMedia& mediaId,
                                    const std::string_view caption = {}) const {
        return sendAnimation_impl(
            replyToMessage->chat->id, ToFileOrString(mediaId), caption,
            createReplyParameters(replyToMessage), nullptr, mode);
    }

    /**
     * @brief Sends an animation message to the specified chat reference
     * message's chat.
     *
     * This function uses the TgBot::Api::sendAnimation method to send an
     * animation message to the chat where the specified `chatReferenceMessage`
     * was sent. The function takes the chat reference message's chat ID and the
     * media ID of the animation to be sent.
     *
     * @param chatReferenceMessage The message that serves as a reference for
     * the chat where the new animation message will be sent.
     * @param mediaId The ID of the animation to be sent.
     * @param caption An optional caption for the animation message.
     *
     * @return A shared pointer to the sent animation message.
     */
    template <ParseMode mode = ParseMode::None>
    Message::Ptr sendAnimation(const ChatIds& chatid,
                               const FileOrMedia& mediaId,
                               const std::string_view caption = {}) const {
        return sendAnimation_impl(chatid, ToFileOrString(mediaId), caption,
                                  nullptr, nullptr, mode);
    }

    Message::Ptr sendReplySticker(const Message::Ptr& replyToMessage,
                                  const FileOrMedia& sticker) const {
        return sendSticker_impl(replyToMessage->chat->id,
                                ToFileOrString(sticker),
                                createReplyParameters(replyToMessage));
    }

    Message::Ptr sendReplyVideo(const Message::Ptr& replyToMessage,
                                const FileOrMedia& video,
                                const std::string_view caption = {}) const {
        return sendVideo_impl(replyToMessage->chat->id, ToFileOrString(video),
                              caption, createReplyParameters(replyToMessage));
    }
    /**
     * @brief Sends a sticker message to the specified chat.
     *
     * This function uses the TgBot::Api::sendSticker method to send a sticker
     * message to the chat specified by the given `chatId`. The function takes
     * the chat ID and the media ID of the sticker to be sent.
     *
     * @param chatId The ID of the chat to which the sticker message will be
     * sent.
     * @param mediaId The ID of the sticker to be sent.
     *
     * @return A shared pointer to the sent sticker message.
     */
    Message::Ptr sendSticker(const ChatIds& chatId,
                             const FileOrMedia& mediaId) const {
        return sendSticker_impl(chatId, ToFileOrString(mediaId));
    }

    template <ParseMode mode = ParseMode::None>
    Message::Ptr editMessage(
        const Message::Ptr& message, const std::string_view newText,
        const TgBot::InlineKeyboardMarkup::Ptr& markup = nullptr) const {
        return editMessage_impl(message, newText, markup, mode);
    }

    inline Message::Ptr editMessageMarkup(
        const StringOrMessage& message,
        const GenericReply::Ptr& replyMarkup) const {
        return editMessageMarkup_impl(message, replyMarkup);
    }

    // Copy message content and reply to it, requires the message isnt deleted.
    inline MessageId copyAndReplyAsMessage(const Message::Ptr& message) const {
        return copyAndReplyAsMessage(message, message);
    }

    inline MessageId copyAndReplyAsMessage(
        const Message::Ptr& message, const Message::Ptr& replyToMessage) const {
        return copyMessage_impl(message->chat->id, message->messageId,
                                createReplyParameters(replyToMessage));
    }

    inline bool answerCallbackQuery(const std::string_view callbackQueryId,
                                    const std::string_view text = {},
                                    bool showAlert = false) const {
        return answerCallbackQuery_impl(callbackQueryId, text, showAlert);
    }

    inline void deleteMessage(const Message::Ptr& message) const {
        deleteMessage_impl(message);
    }

    inline void deleteMessages(ChatId chatid,
                               const std::vector<MessageId>& messages) const {
        deleteMessages_impl(chatid, messages);
    }

    inline void muteChatMember(
        ChatId chatId, UserId userId, TgBot::ChatPermissions::Ptr permissions,
        std::chrono::system_clock::time_point untilDate) const {
        restrictChatMember_impl(chatId, userId, std::move(permissions),
                                untilDate);
    }

    template <ParseMode mode = ParseMode::None>
    Message::Ptr sendDocument(ChatIds chatId, FileOrMedia document,
                              const std::string_view caption = {},
                              GenericReply::Ptr replyMarkup = nullptr) const {
        return sendDocument_impl(chatId, ToFileOrString(std::move(document)),
                                 caption, nullptr, std::move(replyMarkup),
                                 mode);
    }

    template <ParseMode mode = ParseMode::None>
    Message::Ptr sendReplyDocument(
        Message::Ptr message, FileOrMedia document,
        const std::string_view caption = {},
        GenericReply::Ptr replyMarkup = nullptr) const {
        return sendDocument_impl(
            message->chat->id, ToFileOrString(std::move(document)), caption,
            createReplyParameters(message), std::move(replyMarkup), mode);
    }

    template <ParseMode mode = ParseMode::None>
    Message::Ptr sendPhoto(ChatIds chatId, FileOrMedia photo,
                           const std::string_view caption = {},
                           ReplyParametersExt::Ptr replyParameters = nullptr,
                           GenericReply::Ptr replyMarkup = nullptr) const {
        return sendPhoto_impl(chatId, ToFileOrString(std::move(photo)), caption,
                              std::move(replyParameters),
                              std::move(replyMarkup), mode);
    }

    template <ParseMode mode = ParseMode::None>
    Message::Ptr sendVideo(ChatIds chatId, FileOrMedia video,
                           const std::string_view caption = {},
                           ReplyParametersExt::Ptr replyParameters = nullptr,
                           GenericReply::Ptr replyMarkup = nullptr) const {
        return sendVideo_impl(chatId, ToFileOrString(std::move(video)), caption,
                              std::move(replyParameters),
                              std::move(replyMarkup), mode);
    }

    template <ParseMode mode = ParseMode::None>
    Message::Ptr sendReplyPhoto(const Message::Ptr& replyToMessage,
                                const FileOrMedia& photo,
                                const std::string_view caption = {}) const {
        return sendPhoto<mode>(replyToMessage->chat->id, photo, caption,
                               createReplyParameters(replyToMessage));
    }

    [[nodiscard]] inline bool downloadFile(
        const std::filesystem::path& path,
        const std::string_view fileid) const {
        return downloadFile_impl(path, fileid);
    }

    [[nodiscard]] inline User::Ptr getBotUser() const {
        return getBotUser_impl();
    }

    inline Message::Ptr sendDice(const ChatId chat) const {
        return sendDice_impl(chat);
    }

    [[nodiscard]] inline StickerSet::Ptr getStickerSet(
        const std::string_view setName) const {
        return getStickerSet_impl(setName);
    }

    inline bool createNewStickerSet(
        std::int64_t userId, const std::string_view name,
        const std::string_view title,
        const std::vector<InputSticker::Ptr>& stickers,
        Sticker::Type stickerType) const {
        return createNewStickerSet_impl(userId, name, title, stickers,
                                        stickerType);
    }

    [[nodiscard]] inline File::Ptr uploadStickerFile(
        std::int64_t userId, InputFile::Ptr sticker,
        const TgBot::Api::StickerFormat stickerFormat) const {
        return uploadStickerFile_impl(userId, std::move(sticker),
                                      stickerFormat);
    }

    inline bool pinMessage(Message::Ptr message) {
        return pinMessage_impl(std::move(message));
    }
    inline bool unpinMessage(Message::Ptr message) {
        return unpinMessage_impl(std::move(message));
    }
    inline bool banChatMember(const Chat::Ptr& chat, const User::Ptr& user) {
        return banChatMember_impl(chat, user);
    }
    inline bool unbanChatMember(const Chat::Ptr& chat, const User::Ptr& user) {
        return unbanChatMember_impl(chat, user);
    }
    inline User::Ptr getChatMember(const ChatId chat, const UserId user) {
        return getChatMember_impl(chat, user);
    }

    inline void setDescriptions(const std::string_view description,
                                const std::string_view shortDescription) {
        setDescriptions_impl(description, shortDescription);
    }

    inline bool setMessageReaction(
        const Message::Ptr& message,
        const std::vector<ReactionType::Ptr>& reaction, bool isBig) const {
        return setMessageReaction_impl(message->chat->id, message->messageId,
                                       reaction, isBig);
    }

    // TODO: Any better way than this?

    virtual void startPoll() {
        // Dummy implementation
    }
    virtual bool unloadCommand(const std::string& command) {
        return false;  // Dummy implementation
    }
    virtual bool reloadCommand(const std::string& command) {
        return false;  // Dummy implementation
    }

    enum class AnyMessageResult {
        // Handled, keep giving me callbacks
        Handled,
        // Don't give me callbacks, deregister this handler...
        Deregister,
    };

    using AnyMessageCallback =
        std::function<AnyMessageResult(TgBotApi::CPtr, const Message::Ptr&)>;

    virtual void onAnyMessage(const AnyMessageCallback& callback) {
        // Dummy implementation
    }

    virtual void onCallbackQuery(
        std::string message,
        TgBot::EventBroadcaster::CallbackQueryListener listener) {
        // Dummy implementation
    }

    virtual void onEditedMessage(
        TgBot::EventBroadcaster::MessageListener listener) {
        // Dummy implementation
    }

    struct InlineQuery {
        // name of the query, the prefix
        std::string name;
        // Any help text
        std::string description;
        // Command name, if not a command, empty string
        std::string command;
        // hasMoreArguments, if true, consume one of more spaces, otherwise
        // consume none.
        bool hasMoreArguments;
        // Whether it is limited to whitelist users or owners
        bool enforced;

        auto operator<=>(const InlineQuery& other) const = default;
    };
    using InlineCallback =
        std::function<std::vector<TgBot::InlineQueryResult::Ptr>(
            const std::string_view)>;

    virtual void addInlineQueryKeyboard(InlineQuery query,
                                        TgBot::InlineQueryResult::Ptr result) {
        // Dummy implementation
    }
    virtual void addInlineQueryKeyboard(InlineQuery query,
                                        InlineCallback result) {
        // Dummy implementation
    }
    virtual void removeInlineQueryKeyboard(const std::string_view key) {
        // Dummy implementation
    }

   protected:
    static ReplyParametersExt::Ptr createReplyParameters(
        const Message::Ptr& messageToReply) {
        MessageId messageId = messageToReply->messageId;
        std::optional<MessageThreadId> threadId;
        if (messageToReply->isTopicMessage.value_or(false)) {
            threadId = messageToReply->messageThreadId;
        }
        return createReplyParameters(messageId, threadId);
    }

    static ReplyParametersExt::Ptr createReplyParameters(
        MessageId messageId, std::optional<MessageThreadId> messageThreadId) {
        auto ptr = std::make_shared<ReplyParametersExt>();
        ptr->messageId = messageId;
        ptr->allowSendingWithoutReply = true;
        ptr->messageThreadId = messageThreadId;
        return ptr;
    }
};
