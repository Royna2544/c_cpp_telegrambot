#pragma once

#include <Types.h>
#include <tgbot/tgbot.h>
#include <trivial_helpers/_class_helper_macros.h>

#include <Authorization.hpp>
#include <ManagedThreads.hpp>
#include <Random.hpp>
#include <ReplyParametersExt.hpp>
#include <StringResLoader.hpp>
#include <filesystem>
#include <memory>
#include <string_view>
#include <vector>

#include "CommandModule.hpp"
#include "MessageExt.hpp"
#include "Providers.hpp"
#include "TgBotApi.hpp"
#include "api/components/FileCheck.hpp"

using TgBot::Animation;
using TgBot::Api;
using TgBot::Bot;
using TgBot::BotCommand;
using TgBot::Chat;
using TgBot::ChatPermissions;
using TgBot::EventBroadcaster;
using TgBot::File;
using TgBot::GenericReply;
using TgBot::InputFile;
using TgBot::InputSticker;
using TgBot::Message;
using TgBot::Sticker;
using TgBot::StickerSet;
using TgBot::TgLongPoll;
using TgBot::User;

// A class to effectively wrap TgBot::Api to stable interface
// This class owns the Bot instance, and users of this code cannot directly
// access it.
class TgBotApiImpl : public TgBotApi {
   public:
    using Ptr = std::add_pointer_t<TgBotApiImpl>;
    using CPtr = std::add_pointer_t<std::add_const_t<TgBotApiImpl>>;

    // Constructor requires a bot token to create a Bot instance.
    TgBotApiImpl(const std::string_view token, AuthContext* auth,
                 StringResLoaderBase* loader, Providers* providers);
    ~TgBotApiImpl() override;

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
    class ModulesManagement;
    friend class ModulesManagement;
    std::unique_ptr<ModulesManagement> kModuleLoader;
    class RestartCommand;
    friend class RestartCommand;
    std::unique_ptr<RestartCommand> restartCommand;

    std::unique_ptr<FileCheck> virusChecker;
   private:
    /**
     * @brief Sends a message to a chat.
     *
     * @param chatId The unique identifier for the target chat (could be
     * a user or a group chat).
     * @param text The content of the message to be sent.
     * @param replyParameters Optional. Parameters for replying to a
     * message, such as timeouts.
     * @param replyMarkup Optional. Inline keyboard markup for replies.
     * @param parseMode Optional. Defines the parsing mode for the
     * message (e.g., HTML, Markdown).
     *
     * @return A shared pointer to a Message object representing the
     * sent message.
     */
    Message::Ptr sendMessage_impl(
        ChatId chatId, const std::string_view text,
        ReplyParametersExt::Ptr replyParameters, GenericReply::Ptr replyMarkup,
        const std::string_view parseMode) const override;

    /**
     * @brief Sends an animation (GIF or video) to a chat.
     *
     * @param chatId The unique identifier for the target chat.
     * @param animation The animation to be sent. Can be either a file or a URL.
     * @param caption Optional. A caption for the animation.
     * @param replyParameters Optional. Parameters for replying to a message.
     * @param replyMarkup Optional. Inline keyboard markup for replies.
     * @param parseMode Optional. Defines the parsing mode for the caption.
     *
     * @return A shared pointer to a Message object representing the sent
     * animation.
     */
    Message::Ptr sendAnimation_impl(
        ChatId chatId, std::variant<InputFile::Ptr, std::string> animation,
        const std::string_view caption,
        ReplyParametersExt::Ptr replyParameters = nullptr,
        GenericReply::Ptr replyMarkup = nullptr,
        const std::string_view parseMode = {}) const override;

    /**
     * @brief Sends a sticker to a chat.
     *
     * @param chatId The unique identifier for the target chat.
     * @param sticker The sticker to be sent, either as a file or a URL.
     * @param replyParameters Optional. Parameters for replying to a message.
     *
     * @return A shared pointer to a Message object representing the sent
     * sticker.
     */
    Message::Ptr sendSticker_impl(
        ChatId chatId, std::variant<InputFile::Ptr, std::string> sticker,
        ReplyParametersExt::Ptr replyParameters) const override;

    /**
     * @brief Edits a message with new text and an optional inline keyboard.
     *
     * @param message A shared pointer to the message to be edited.
     * @param newText The new text for the message.
     * @param markup The inline keyboard markup for the edited message.
     * @param parseMode Defines the parsing mode for the new text (e.g., HTML,
     * Markdown).
     *
     * @return A shared pointer to the edited Message object.
     */
    Message::Ptr editMessage_impl(
        const Message::Ptr& message, const std::string_view newText,
        const TgBot::InlineKeyboardMarkup::Ptr& markup,
        const std::string_view parseMode) const override;

    /**
     * @brief Edits the inline keyboard markup of a message.
     *
     * @param message The message or string identifier to edit.
     * @param markup The new inline keyboard markup.
     *
     * @return A shared pointer to the updated Message object.
     */
    Message::Ptr editMessageMarkup_impl(
        const StringOrMessage& message,
        const GenericReply::Ptr& markup) const override;

    /**
     * @brief Copies a message to another chat.
     *
     * @param fromChatId The unique identifier for the source chat.
     * @param messageId The ID of the message to copy.
     * @param replyParameters Optional. Parameters for replying to a message.
     *
     * @return The ID of the copied message.
     */
    MessageId copyMessage_impl(
        ChatId fromChatId, MessageId messageId,
        ReplyParametersExt::Ptr replyParameters = nullptr) const override;

    /**
     * @brief Answers a callback query from an inline keyboard.
     *
     * @param callbackQueryId The ID of the callback query to answer.
     * @param text Optional. Text to be displayed in the callback answer.
     * @param showAlert Optional. If true, an alert is shown to the user.
     *
     * @return True if the callback query was answered successfully, otherwise
     * false.
     */
    bool answerCallbackQuery_impl(const std::string_view callbackQueryId,
                                  const std::string_view text = {},
                                  bool showAlert = false) const override;

    /**
     * @brief Deletes a message from a chat.
     *
     * @param message A shared pointer to the message to delete.
     */
    void deleteMessage_impl(const Message::Ptr& message) const override;

    /**
     * @brief Deletes a range of messages from a chat.
     *
     * @param chatId The unique identifier for the target chat.
     * @param messageIds A vector of message IDs to be deleted.
     */
    void deleteMessages_impl(
        ChatId chatId, const std::vector<MessageId>& messageIds) const override;

    /**
     * @brief Mutes a chat member by restricting their permissions.
     *
     * @param chatId The unique identifier for the target chat.
     * @param userId The unique identifier for the user to restrict.
     * @param permissions A shared pointer to the chat permissions object.
     * @param untilDate The date until the restrictions apply (in Unix timestamp
     * format).
     */
    void restrictChatMember_impl(ChatId chatId, UserId userId,
                                 TgBot::ChatPermissions::Ptr permissions,
                                 std::uint32_t untilDate) const override;

    /**
     * @brief Sends a document (file) to a chat.
     *
     * @param chatId The unique identifier for the target chat.
     * @param document The document to be sent, either as a file or a URL.
     * @param caption Optional. A caption for the document.
     * @param replyParameters Optional. Parameters for replying to a message.
     * @param replyMarkup Optional. Inline keyboard markup for replies.
     * @param parseMode Optional. Defines the parsing mode for the caption.
     *
     * @return A shared pointer to a Message object representing the sent
     * document.
     */
    Message::Ptr sendDocument_impl(
        ChatId chatId, FileOrString document, const std::string_view caption,
        ReplyParametersExt::Ptr replyParameters = nullptr,
        GenericReply::Ptr replyMarkup = nullptr,
        const std::string_view parseMode = {}) const override;

    /**
     * @brief Sends a photo to a chat.
     *
     * @param chatId The unique identifier for the target chat.
     * @param photo The photo to be sent, either as a file or a URL.
     * @param caption Optional. A caption for the photo.
     * @param replyParameters Optional. Parameters for replying to a message.
     * @param replyMarkup Optional. Inline keyboard markup for replies.
     * @param parseMode Optional. Defines the parsing mode for the caption.
     *
     * @return A shared pointer to a Message object representing the sent photo.
     */
    Message::Ptr sendPhoto_impl(
        ChatId chatId, FileOrString photo, const std::string_view caption,
        ReplyParametersExt::Ptr replyParameters = nullptr,
        GenericReply::Ptr replyMarkup = nullptr,
        const std::string_view parseMode = {}) const override;

    /**
     * @brief Sends a video to a chat.
     *
     * @param chatId The unique identifier for the target chat.
     * @param video The video to be sent, either as a file or a URL.
     * @param caption Optional. A caption for the video.
     * @param replyParameters Optional. Parameters for replying to a message.
     * @param replyMarkup Optional. Inline keyboard markup for replies.
     * @param parseMode Optional. Defines the parsing mode for the caption.
     *
     * @return A shared pointer to a Message object representing the sent video.
     */
    Message::Ptr sendVideo_impl(
        ChatId chatId, FileOrString video, const std::string_view caption,
        ReplyParametersExt::Ptr replyParameters = nullptr,
        GenericReply::Ptr replyMarkup = nullptr,
        const std::string_view parseMode = {}) const override;

    /**
     * @brief Sends a dice roll to a chat.
     *
     * @param chatId The unique identifier for the target chat.
     *
     * @return A shared pointer to a Message object representing the sent dice
     * roll.
     */
    Message::Ptr sendDice_impl(ChatId chatId) const override;

    /**
     * @brief Retrieves a sticker set by its name.
     *
     * @param setName The name of the sticker set.
     *
     * @return A shared pointer to a StickerSet object representing the
     * requested set.
     */
    StickerSet::Ptr getStickerSet_impl(
        const std::string_view setName) const override;

    /**
     * @brief Creates a new sticker set for a user.
     *
     * @param userId The unique identifier of the user creating the set.
     * @param name The name of the new sticker set.
     * @param title The title of the new sticker set.
     * @param stickers A vector of stickers to be included in the set.
     * @param stickerType The type of stickers in the set.
     *
     * @return True if the sticker set was created successfully, otherwise
     * false.
     */
    bool createNewStickerSet_impl(
        UserId userId, const std::string_view name,
        const std::string_view title,
        const std::vector<InputSticker::Ptr>& stickers,
        Sticker::Type stickerType) const override;

    /**
     * @brief Uploads a sticker file for a user.
     *
     * @param userId The unique identifier of the user owning the sticker.
     * @param sticker The sticker file to upload.
     * @param stickerFormat The format of the sticker file.
     *
     * @return A shared pointer to a File object representing the uploaded
     * sticker.
     */
    File::Ptr uploadStickerFile_impl(
        UserId userId, InputFile::Ptr sticker,
        const std::string_view stickerFormat) const override;

    /**
     * @brief Downloads a file from the Telegram server.
     *
     * @param destfilename The destination file path where the file will be
     * saved.
     * @param fileid The ID of the file to be downloaded.
     *
     * @return True if the file was downloaded successfully, otherwise false.
     */
    bool downloadFile_impl(const std::filesystem::path& destfilename,
                           const std::string_view fileid) const override;

    /**
     * @brief Retrieves the bot's user object.
     *
     * @return A shared pointer to the bot's User object.
     */
    User::Ptr getBotUser_impl() const override;

    /**
     * @brief Pins a message in a chat.
     *
     * @param message A shared pointer to the message to pin.
     *
     * @return True if the message was pinned successfully, otherwise false.
     */
    bool pinMessage_impl(Message::Ptr message) const override;

    /**
     * @brief Unpins a message in a chat.
     *
     * @param message A shared pointer to the message to unpin.
     *
     * @return True if the message was unpinned successfully, otherwise false.
     */
    bool unpinMessage_impl(Message::Ptr message) const override;

    /**
     * @brief Bans a user from a chat.
     *
     * @param chat The chat where the user should be banned.
     * @param user The user to ban.
     *
     * @return True if the user was banned successfully, otherwise false.
     */
    bool banChatMember_impl(const Chat::Ptr& chat,
                            const User::Ptr& user) const override;

    /**
     * @brief Unbans a user in a chat.
     *
     * @param chat The chat where the user should be unbanned.
     * @param user The user to unban.
     *
     * @return True if the user was unbanned successfully, otherwise false.
     */
    bool unbanChatMember_impl(const Chat::Ptr& chat,
                              const User::Ptr& user) const override;

    /**
     * @brief Retrieves information about a chat member.
     *
     * @param chat The unique identifier for the target chat.
     * @param user The unique identifier for the target user.
     *
     * @return A shared pointer to a User object representing the chat member.
     */
    User::Ptr getChatMember_impl(ChatId chat, UserId user) const override;

    /**
     * @brief Sets the descriptions of a chat.
     *
     * @param description The long description of the chat.
     * @param shortDescription The short description of the chat.
     */
    void setDescriptions_impl(
        const std::string_view description,
        const std::string_view shortDescription) const override;

    /**
     * @brief Sets a reaction to a message.
     *
     * @param chatid The unique identifier for the target chat.
     * @param message The unique identifier for the target message.
     * @param reaction A list of reaction types to apply to the message.
     * @param isBig Optional. If true, the reaction is displayed as a larger
     * icon.
     *
     * @return True if the reaction was set successfully, otherwise false.
     */
    bool setMessageReaction_impl(const ChatId chatid, const MessageId message,
                                 const std::vector<ReactionType::Ptr>& reaction,
                                 bool isBig) const override;

    // A global link preview options
    TgBot::LinkPreviewOptions::Ptr globalLinkOptions;

   public:
    void startPoll() override;

    bool unloadCommand(const std::string& command) override;
    bool reloadCommand(const std::string& command) override;
    void commandHandler(const std::string& command,
                        AuthContext::Flags authflags, Message::Ptr message);
    bool validateValidArgs(const DynModule* module, MessageExt::Ptr& message);

    template <unsigned Len>
    static auto getInitCallNameForClient(const char (&str)[Len]) {
        return fmt::format("Register onAnyMessage callbacks: {}", str);
    }

    void addInlineQueryKeyboard(InlineQuery query,
                                TgBot::InlineQueryResult::Ptr result) override;
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

    void onCallbackQuery(
        std::string command,
        TgBot::EventBroadcaster::CallbackQueryListener listener) override;

   private:
    [[nodiscard]] EventBroadcaster& getEvents() { return _bot.getEvents(); }
    [[nodiscard]] const Api& getApi() const { return _bot.getApi(); }

    [[nodiscard]] bool authorized(const MessageExt::Ptr& message,
                                  const std::string_view commandName,
                                  AuthContext::Flags flags) const;

    [[nodiscard]] bool isMyCommand(const MessageExt::Ptr& message) const;

    class Async;
    Bot _bot;

    AuthContext* _auth;
    StringResLoaderBase* _loader;
    Providers* _provider;
};
