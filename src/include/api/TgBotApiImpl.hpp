#pragma once

#include <trivial_helpers/_class_helper_macros.h>

#include <ManagedThreads.hpp>
#include <api/types/LinkPreviewOptions.hpp>
#include <filesystem>
#include <memory>
#include <string_view>
#include <tgbotxx/Bot.hpp>
#include <vector>

#include "AuthContext.hpp"
#include "CommandModule.hpp"
#include "Providers.hpp"
#include "RateLimit.hpp"
#include "RefLock.hpp"
#include "StringResLoader.hpp"
#include "TgBotApi.hpp"

// A class to effectively wrap TgBot::Api to stable interface
// This class owns the Bot instance, and users of this code cannot directly
// access it.
class TgBotApiImpl : public TgBotApi, tgbotxx::Bot {
   public:
    using Ptr = std::add_pointer_t<TgBotApiImpl>;
    using CPtr = std::add_pointer_t<std::add_const_t<TgBotApiImpl>>;

    // Constructor requires a bot token to create a Bot instance.
    TgBotApiImpl(const std::string_view token, AuthContext* auth,
                 StringResLoader* loader, Providers* providers,
                 RefLock* refLock);
    ~TgBotApiImpl() override;

    class ModulesManagement;
    friend class ModulesManagement;
    std::unique_ptr<ModulesManagement> kModuleLoader;
    class OnAnyMessageImpl;
    friend class OnAnyMessageImpl;
    std::unique_ptr<OnAnyMessageImpl> onAnyMessageImpl;
    class OnCallbackQueryImpl;
    friend class OnCallbackQueryImpl;
    std::unique_ptr<OnCallbackQueryImpl> onCallbackQueryImpl;
    class OnUnknownCommandImpl;
    friend class OnUnknownCommandImpl;
    std::unique_ptr<OnUnknownCommandImpl> onUnknownCommandImpl;
    class OnInlineQueryImpl;
    friend class OnInlineQueryImpl;
    std::unique_ptr<OnInlineQueryImpl> onInlineQueryImpl;
    class ChatJoinRequestImpl;
    friend class ChatJoinRequestImpl;
    std::unique_ptr<ChatJoinRequestImpl> onChatJoinRequestImpl;
    class OnMyChatMemberImpl;
    friend class OnMyChatMemberImpl;
    std::unique_ptr<OnMyChatMemberImpl> onMyChatMemberImpl;
    class RestartCommand;
    friend class RestartCommand;
    std::unique_ptr<RestartCommand> restartCommand;

    // Interface for listening to command unload/reload
    struct CommandListener {
        ~CommandListener() = default;
        virtual void onUnload(const std::string_view name) = 0;
        virtual void onReload(const std::string_view name) = 0;
    };

    /**
     * @brief Adds a command unload/reload listener
     *
     * @param listener The listener to add.
     */
    void addCommandListener(CommandListener* listener);

   private:
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
    std::optional<api::types::Message> sendMessage_impl(
        api::types::Chat::id_type chatId, const std::string_view text,
        std::optional<api::types::ReplyParameters> replyParameters =
            std::nullopt,
        std::optional<api::types::GenericReply> replyMarkup = std::nullopt,
        const ParseMode parseMode = {}) const override;

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
    std::optional<api::types::Message> sendAnimation_impl(
        api::types::Chat::id_type chatId, FileOrString animation,
        const std::string_view caption = {},
        std::optional<api::types::ReplyParameters> replyParameters =
            std::nullopt,
        std::optional<api::types::GenericReply> replyMarkup = std::nullopt,
        const ParseMode parseMode = {}) const override;

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
    std::optional<api::types::Message> sendSticker_impl(
        api::types::Chat::id_type chatId, FileOrString sticker,
        std::optional<api::types::ReplyParameters> replyParameters =
            std::nullopt) const override;

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
    bool createNewStickerSet_impl(
        std::int64_t userId, const std::string_view name,
        const std::string_view title,
        const std::vector<api::types::InputSticker>& stickers,
        StickerType stickerType) const override;

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
    std::optional<api::types::File> uploadStickerFile_impl(
        std::int64_t userId, api::types::InputFile sticker,
        const StickerFormat stickerFormat) const override;

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
    std::optional<api::types::Message> editMessage_impl(
        const std::optional<api::types::Message>& message,
        const std::string_view newText,
        const std::optional<api::types::InlineKeyboardMarkup> markup,
        const ParseMode parseMode) const override;

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
    std::optional<api::types::Message> editMessageMarkup_impl(
        const StringOrMessage& message,
        const api::types::GenericReply& markup) const override;

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
    api::types::Message::messageId_type copyMessage_impl(
        api::types::Chat::id_type fromChatId,
        api::types::Message::messageId_type messageId,
        std::optional<api::types::ReplyParameters> replyParameters =
            std::nullopt) const override;

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
    bool answerCallbackQuery_impl(const std::string_view callbackQueryId,
                                  const std::string_view text = {},
                                  bool showAlert = false) const override;

    /**
     * @brief Deletes a sent message.
     *
     * This function deletes a previously sent message from the chat.
     *
     * @param message The message to be deleted.
     */
    void deleteMessage_impl(
        const std::optional<api::types::Message>& message) const override;

    /**
     * @brief Deletes multiple messages in a chat.
     *
     * This function deletes multiple messages in bulk from the specified chat.
     *
     * @param chatId The ID of the chat from which the messages will be deleted.
     * @param messageIds The vector of message IDs to be deleted.
     */
    void deleteMessages_impl(
        api::types::Chat::id_type chatId,
        const std::vector<api::types::Message::messageId_type>& messageIds)
        const override;

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
    void restrictChatMember_impl(
        api::types::Chat::id_type chatId, api::types::User::id_type userId,
        api::types::ChatPermissions permissions,
        std::chrono::system_clock::time_point untilDate = {}) const override;

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
    std::optional<api::types::Message> sendDocument_impl(
        api::types::Chat::id_type chatId, FileOrString document,
        const std::string_view caption = {},
        std::optional<api::types::ReplyParameters> replyParameters =
            std::nullopt,
        std::optional<api::types::GenericReply> replyMarkup = std::nullopt,
        const ParseMode parseMode = {}) const override;

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
    std::optional<api::types::Message> sendPhoto_impl(
        api::types::Chat::id_type chatId, FileOrString photo,
        const std::string_view caption = {},
        std::optional<api::types::ReplyParameters> replyParameters =
            std::nullopt,
        std::optional<api::types::GenericReply> replyMarkup = std::nullopt,
        const ParseMode parseMode = {}) const override;

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
    std::optional<api::types::Message> sendVideo_impl(
        api::types::Chat::id_type chatId, FileOrString video,
        const std::string_view caption = {},
        std::optional<api::types::ReplyParameters> replyParameters =
            std::nullopt,
        std::optional<api::types::GenericReply> replyMarkup = std::nullopt,
        const ParseMode parseMode = {}) const override;

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
    std::optional<api::types::Message> sendDice_impl(
        api::types::Chat::id_type chatId) const override;

    /**
     * @brief Retrieves information about a sticker set.
     *
     * This function gets a sticker set by its name.
     *
     * @param setName The name of the sticker set to be retrieved.
     *
     * @return A StickerSet object containing information about the sticker set.
     */
    api::types::StickerSet getStickerSet_impl(
        const std::string_view setName) const override;

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
    bool downloadFile_impl(const std::filesystem::path& destFilename,
                           const std::string_view fileId) const override;

    /**
     * @brief Retrieves information about the bot user.
     *
     * This function gets the bot's own user information.
     *
     * @return An optional User object representing the bot user.
     */
    std::optional<api::types::User> getBotUser_impl() const override;

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
    bool pinMessage_impl(
        std::optional<api::types::Message> message) const override;

    /**
     * @brief Unpins a message in a chat.
     *
     * This function removes the pinned status from a previously pinned message.
     *
     * @param message The message to be unpinned.
     *
     * @return True if the message was unpinned successfully, false otherwise.
     */
    bool unpinMessage_impl(
        std::optional<api::types::Message> message) const override;

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
    bool banChatMember_impl(
        const api::types::Chat& chat,
        const std::optional<api::types::User>& user) const override;

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
    bool unbanChatMember_impl(
        const api::types::Chat& chat,
        const std::optional<api::types::User>& user) const override;

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
    std::optional<api::types::User> getChatMember_impl(
        const api::types::Chat::id_type chat,
        const api::types::User::id_type userId) const override;

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
    void setDescriptions_impl(
        const std::optional<std::string_view> description,
        const std::optional<std::string_view> shortDescription) const override;

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
    bool setMessageReaction_impl(
        api::types::Chat::id_type chatId,
        api::types::Message::messageId_type messageId,
        const std::vector<api::types::ReactionType>& reaction,
        bool isBig) const override;

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
    bool setChatAdministratorCustomTitle(
        api::types::Chat::id_type chatId, api::types::User::id_type userId,
        const std::string_view customTitle) const override;

    /**
     * @brief Sets the list of the bot's commands.
     *
     * @param commands A vector of BotCommand objects representing the bot's
     * commands.
     * @return True if the commands were successfully set, false otherwise.
     */
    bool setMyCommands_impl(
        const std::vector<api::types::BotCommand>& commands) const override;

    /**
     * @brief Retrieves information about a chat.
     *
     * @param chatId The unique identifier of the chat to retrieve.
     *
     * @return An optional Chat object representing the chat information.
     */
    std::optional<api::types::Chat> getChat_impl(
        api::types::Chat::id_type chatId) const override;

    // A global link preview options
    api::types::LinkPreviewOptions globalLinkOptions;

   public:
    void startPoll() override;

    bool unloadCommand(const std::string& command) override;
    bool reloadCommand(const std::string& command) override;
    void commandHandler(const std::string& command,
                        AuthContext::AccessLevel authflags,
                        api::types::Message message);
    bool validateValidArgs(const CommandModule::Info* module,
                           api::types::ParsedMessage* message);

    void addInlineQueryKeyboard(InlineQuery query,
                                api::types::InlineQueryResult result) override;
    void addInlineQueryKeyboard(InlineQuery query,
                                InlineCallback result) override;
    void removeInlineQueryKeyboard(const std::string_view key) override;

    /**
     * @brief Registers a callback function to be called when any message is
     * received.
     *
     * @param callback The function to be called when any message is
     * received.
     */
    void onAnyMessage(const AnyMessageCallback& callback) override;

    void onCallbackQuery(std::string command,
                         CallbackQueryCallback listener) override;

   private:
    [[nodiscard]] bool authorized(const api::types::ParsedMessage* message,
                                  const std::string_view commandName,
                                  AuthContext::AccessLevel flags) const;

    [[nodiscard]] bool isMyCommand(
        const api::types::ParsedMessage* message) const;

    class Async;

    mutable api::types::User me;  // Just a cache

    AuthContext* _auth;
    StringResLoader* _loader;
    Providers* _provider;
    std::vector<CommandListener*> _listeners;
    IntervalRateLimiter _rateLimiter;
    RefLock* _refLock;
};
