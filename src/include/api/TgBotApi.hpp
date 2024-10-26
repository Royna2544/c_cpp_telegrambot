#pragma once

#include <tgbot/EventBroadcaster.h>
#include <tgbot/types/InlineQueryResult.h>
#include <tgbot/types/InputFile.h>
#include <tgbot/types/InputSticker.h>
#include <tgbot/types/Message.h>
#include <tgbot/types/Sticker.h>
#include <tgbot/types/StickerSet.h>
#include <trivial_helpers/fruit_inject.hpp>

#include <ReplyParametersExt.hpp>
#include <boost/variant.hpp>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "Utils.hpp"

using TgBot::Chat;
using TgBot::File;
using TgBot::GenericReply;
using TgBot::InputFile;
using TgBot::InputSticker;
using TgBot::Message;
using TgBot::Sticker;
using TgBot::StickerSet;
using TgBot::User;

// Base interface for operations involving TgBot...
class TgBotApi {
   public:
    using Ptr = std::add_pointer_t<TgBotApi>;
    using CPtr = std::add_pointer_t<std::add_const_t<TgBotApi>>;

    APPLE_INJECT(TgBotApi()) = default;
    virtual ~TgBotApi() = default;

    // Disable copy and move constructors
    TgBotApi(const TgBotApi&) = delete;
    TgBotApi(TgBotApi&&) = delete;
    TgBotApi& operator=(const TgBotApi&) = delete;
    TgBotApi& operator=(TgBotApi&&) = delete;

    using FileOrString = boost::variant<InputFile::Ptr, std::string>;
    using FileOrMedia = boost::variant<InputFile::Ptr, MediaIds>;
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
        ChatId chatId, StringOrView text,
        ReplyParametersExt::Ptr replyParameters = nullptr,
        GenericReply::Ptr replyMarkup = nullptr,
        StringOrView parseMode = {}) const = 0;

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
        ChatId chatId, FileOrString animation, StringOrView caption = {},
        ReplyParametersExt::Ptr replyParameters = nullptr,
        GenericReply::Ptr replyMarkup = nullptr,
        StringOrView parseMode = {}) const = 0;

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
        std::int64_t userId, StringOrView name, StringOrView title,
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
        StringOrView stickerFormat) const = 0;

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
        const Message::Ptr& message, StringOrView newText,
        const TgBot::InlineKeyboardMarkup::Ptr& markup,
        StringOrView parseMode) const = 0;

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
    virtual bool answerCallbackQuery_impl(StringOrView callbackQueryId,
                                          StringOrView text = {},
                                          bool showAlert = false) const = 0;

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
        std::uint32_t untilDate = 0) const = 0;

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
        ChatId chatId, FileOrString document, StringOrView caption = {},
        ReplyParametersExt::Ptr replyParameters = nullptr,
        GenericReply::Ptr replyMarkup = nullptr,
        StringOrView parseMode = {}) const = 0;

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
        ChatId chatId, FileOrString photo, StringOrView caption = {},
        ReplyParametersExt::Ptr replyParameters = nullptr,
        GenericReply::Ptr replyMarkup = nullptr,
        StringOrView parseMode = {}) const = 0;

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
        ChatId chatId, FileOrString video, StringOrView caption = {},
        ReplyParametersExt::Ptr replyParameters = nullptr,
        GenericReply::Ptr replyMarkup = nullptr,
        StringOrView parseMode = {}) const = 0;

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
    virtual StickerSet::Ptr getStickerSet_impl(StringOrView setName) const = 0;

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
                                   StringOrView fileId) const = 0;

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

    virtual void setDescriptions_impl(StringOrView description,
                                      StringOrView shortDescription) const = 0;

    static FileOrString ToFileOrString(const FileOrMedia& media) {
        if (media.which() == 0) {
            return boost::get<InputFile::Ptr>(media);
        } else {
            return boost::get<MediaIds>(media).id;
        }
    }

   public:
    // Convience wrappers over real API
    enum class ParseMode {
        Markdown,
        HTML,
        None,
    };

    template <ParseMode mode>
    static consteval std::string_view parseModeToStr() {
        switch (mode) {
            case ParseMode::Markdown:
                return "Markdown";
            case ParseMode::HTML:
                return "HTML";
            case ParseMode::None:
                return {};
        }
        return "Unknown";
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
        const Message::Ptr& replyToMessage, StringOrView message,
        const GenericReply::Ptr& replyMarkup = nullptr) const {
        MessageThreadId tid = replyToMessage->messageThreadId;
        if (!replyToMessage->chat->isForum) {
            tid = 0;
        }
        return sendReplyMessage<mode>(replyToMessage->chat->id,
                                      replyToMessage->messageId, tid, message,
                                      replyMarkup);
    }

    template <ParseMode mode = ParseMode::None>
    Message::Ptr sendReplyMessage(
        const ChatId chatId, const MessageId messageId,
        const MessageThreadId messageTid, StringOrView message,
        const GenericReply::Ptr& replyMarkup = nullptr) const {
        auto params = std::make_shared<ReplyParametersExt>();
        params->messageId = messageId;
        params->chatId = chatId;
        params->messageThreadId = messageTid;
        params->allowSendingWithoutReply = true;
        return sendMessage_impl(chatId, message, params, replyMarkup,
                                parseModeToStr<mode>());
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
    Message::Ptr sendMessage(const ChatIds& chatId, StringOrView message,
                             const GenericReply::Ptr& markup = nullptr) const {
        return sendMessage_impl(chatId, message, nullptr, markup,
                                parseModeToStr<mode>());
    }

    template <ParseMode mode = ParseMode::None>
    Message::Ptr sendReplyAnimation(const Message::Ptr& replyToMessage,
                                    const FileOrMedia& mediaId,
                                    StringOrView caption = {}) const {
        return sendAnimation_impl(replyToMessage->chat->id,
                                  ToFileOrString(mediaId), caption,
                                  createReplyParametersForReply(replyToMessage),
                                  nullptr, parseModeToStr<mode>());
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
                               StringOrView caption = {}) const {
        return sendAnimation_impl(chatid, ToFileOrString(mediaId), caption,
                                  nullptr, nullptr, parseModeToStr<mode>());
    }

    Message::Ptr sendReplySticker(const Message::Ptr& replyToMessage,
                                  const FileOrMedia& sticker) const {
        return sendSticker_impl(replyToMessage->chat->id,
                                ToFileOrString(sticker),
                                createReplyParametersForReply(replyToMessage));
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
        const Message::Ptr& message, StringOrView newText,
        const TgBot::InlineKeyboardMarkup::Ptr& markup = nullptr) const {
        return editMessage_impl(message, newText, markup,
                                parseModeToStr<mode>());
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
                                createReplyParametersForReply(replyToMessage));
    }

    [[nodiscard]] inline bool answerCallbackQuery(
        StringOrView callbackQueryId, StringOrView text = {},
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

    inline void muteChatMember(ChatId chatId, UserId userId,
                               TgBot::ChatPermissions::Ptr permissions,
                               std::uint32_t untilDate) const {
        restrictChatMember_impl(chatId, userId, std::move(permissions),
                                untilDate);
    }

    template <ParseMode mode = ParseMode::None>
    Message::Ptr sendDocument(ChatIds chatId, FileOrMedia document,
                              StringOrView caption = {},
                              ReplyParametersExt::Ptr replyParameters = nullptr,
                              GenericReply::Ptr replyMarkup = nullptr) const {
        return sendDocument_impl(chatId, ToFileOrString(document), caption,
                                 std::move(replyParameters),
                                 std::move(replyMarkup),
                                 parseModeToStr<mode>());
    }

    template <ParseMode mode = ParseMode::None>
    Message::Ptr sendPhoto(ChatIds chatId, const FileOrMedia& photo,
                           StringOrView caption = {},
                           ReplyParametersExt::Ptr replyParameters = nullptr,
                           GenericReply::Ptr replyMarkup = nullptr) const {
        return sendPhoto_impl(chatId, ToFileOrString(photo), caption,
                              std::move(replyParameters),
                              std::move(replyMarkup), parseModeToStr<mode>());
    }

    template <ParseMode mode = ParseMode::None>
    Message::Ptr sendVideo(ChatIds chatId, const FileOrMedia& video,
                           StringOrView caption = {},
                           ReplyParametersExt::Ptr replyParameters = nullptr,
                           GenericReply::Ptr replyMarkup = nullptr) const {
        return sendVideo_impl(chatId, ToFileOrString(video), caption,
                              std::move(replyParameters),
                              std::move(replyMarkup), parseModeToStr<mode>());
    }

    template <ParseMode mode = ParseMode::None>
    Message::Ptr sendReplyPhoto(const Message::Ptr& replyToMessage,
                                const FileOrMedia& photo,
                                StringOrView caption = {}) const {
        return sendPhoto<mode>(replyToMessage->chat->id, photo, caption);
    }

    [[nodiscard]] inline bool downloadFile(const std::filesystem::path& path,
                                           StringOrView fileid) const {
        return downloadFile_impl(path, fileid);
    }

    [[nodiscard]] inline User::Ptr getBotUser() const {
        return getBotUser_impl();
    }

    inline Message::Ptr sendDice(const ChatId chat) const {
        return sendDice_impl(chat);
    }

    [[nodiscard]] inline StickerSet::Ptr getStickerSet(
        StringOrView setName) const {
        return getStickerSet_impl(setName);
    }

    [[nodiscard]] inline bool createNewStickerSet(
        std::int64_t userId, StringOrView name, StringOrView title,
        const std::vector<InputSticker::Ptr>& stickers,
        Sticker::Type stickerType) const {
        return createNewStickerSet_impl(userId, name, title, stickers,
                                        stickerType);
    }

    [[nodiscard]] inline File::Ptr uploadStickerFile(
        std::int64_t userId, InputFile::Ptr sticker,
        StringOrView stickerFormat) const {
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

    inline void setDescriptions(StringOrView description,
                                StringOrView shortDescription) {
        setDescriptions_impl(std::move(description),
                             std::move(shortDescription));
    }

    // TODO: Any better way than this?

    virtual void startPoll() {
        // Dummy implementation
    }
    virtual bool unloadCommand(StringOrView command) {
        return false;  // Dummy implementation
    }
    virtual bool reloadCommand(StringOrView command) {
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
        StringOrView message,
        const TgBot::EventBroadcaster::CallbackQueryListener& listener) {
        // Dummy implementation
    }

    struct InlineQuery {
        // name of the query, the prefix
        std::string name;
        // Any help text
        std::string description;
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
    virtual void removeInlineQueryKeyboard(StringOrView key) {
        // Dummy implementation
    }

   protected:
    static ReplyParametersExt::Ptr createReplyParametersForReply(
        const Message::Ptr& message) {
        auto ptr = std::make_shared<ReplyParametersExt>();
        ptr->messageId = message->messageId;
        ptr->chatId = message->chat->id;
        ptr->allowSendingWithoutReply = true;
        if (!message->chat->isForum) {
            ptr->messageThreadId = 0;
        } else {
            ptr->messageThreadId = message->messageThreadId;
        }
        return ptr;
    }
};