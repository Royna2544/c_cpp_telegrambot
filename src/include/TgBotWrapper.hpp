#pragma once

#include <Authorization.h>
#include <TgBotPPImpl_shared_depsExports.h>
#include <tgbot/tgbot.h>

#include <boost/algorithm/string/trim.hpp>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <variant>

#include "CompileTimeStringConcat.hpp"
#include "InstanceClassBase.hpp"
#include "Random.hpp"
#include "Types.h"

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
using TgBot::ReplyParameters;
using TgBot::Sticker;
using TgBot::StickerSet;
using TgBot::TgLongPoll;
using TgBot::User;

struct MessageWrapper;
struct MessageWrapperLimited;
// API part wrapper (base)
struct TgBotApi;

using MessagePtr = const Message::Ptr&;
using ApiPtr = const std::shared_ptr<TgBotApi>&;

#define DYN_COMMAND_SYM_STR "loadcmd"
#define DYN_COMMAND_SYM loadcmd
#define DYN_COMMAND_FN(n, m) \
    extern "C" bool DYN_COMMAND_SYM(const char* n, CommandModule& m)
#define COMMAND_HANDLER_NAME(cmd) handle_command_##cmd
#define DECLARE_COMMAND_HANDLER(cmd, w, m) \
    void COMMAND_HANDLER_NAME(cmd)(ApiPtr w, MessagePtr m)

using onanymsg_callback_type = std::function<void(ApiPtr, MessagePtr)>;
using command_callback_t = std::function<void(ApiPtr wrapper, MessagePtr)>;

struct CommandModule : TgBot::BotCommand {
    enum Flags { None = 0, Enforced = 1 << 0, HideDescription = 1 << 1 };
    command_callback_t fn;
    unsigned int flags{};
    bool isLoaded = false;
    CommandModule(const std::string& name, const std::string& description,
                  command_callback_t fn, unsigned int flags) noexcept
        : fn(std::move(fn)), flags(flags) {
        this->command = name;
        this->description = description;
    }
    CommandModule() = default;

    [[nodiscard]] constexpr bool isEnforced() const {
        return (flags & Enforced) != 0;
    }
    [[nodiscard]] bool isHideDescription() const {
        return (flags & HideDescription) != 0;
    }
};

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
    bool operator==(const MediaIds& other) const {
        return id == other.id && uniqueid == other.uniqueid;
    }

    /**
     * @brief Compares two MediaIds objects for inequality.
     *
     * @param other The MediaIds object to compare with this one.
     * @return True if the IDs and unique IDs of both objects are not equal,
     * false otherwise.
     */
    bool operator!=(const MediaIds& other) const { return !(*this == other); }

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
    ChatIds(ChatId id) : _id(id) {}  // NOLINT(hicpp-explicit-conversions)
    ChatIds(MessagePtr message)
        : _id(message->chat->id) {}  // NOLINT(hicpp-explicit-conversions)
    operator ChatId() const {
        return _id;
    }  // NOLINT(hicpp-explicit-conversions)
    ChatId _id;
};

// Base interface for operations involving TgBot...
struct TgBotApi {
   public:
    TgBotApi() = default;
    virtual ~TgBotApi() = default;
    using FileOrString = boost::variant<InputFile::Ptr, std::string>;
    using FileOrMedia = boost::variant<InputFile::Ptr, MediaIds>;
    using StringOrMessage = std::variant<std::string, Message::Ptr>;

   protected:
    // Methods to be implemented

    // Send a message to the chat
    virtual Message::Ptr sendMessage_impl(
        ChatId chatId, const std::string& text,
        ReplyParameters::Ptr replyParameters = nullptr,
        GenericReply::Ptr replyMarkup = nullptr,
        const std::string& parseMode = "") const = 0;

    // Send a GIF to the chat
    virtual Message::Ptr sendAnimation_impl(
        ChatId chatId, FileOrString animation, const std::string& caption,
        ReplyParameters::Ptr replyParameters = nullptr,
        GenericReply::Ptr replyMarkup = nullptr,
        const std::string& parseMode = "") const = 0;

    // Send a sticker to the chat
    virtual Message::Ptr sendSticker_impl(
        ChatId chatId, FileOrString sticker,
        ReplyParameters::Ptr replyParameters = nullptr) const = 0;

    // Create a new sticker set
    virtual bool createNewStickerSet_impl(
        std::int64_t userId, const std::string& name, const std::string& title,
        const std::vector<InputSticker::Ptr>& stickers,
        Sticker::Type stickerType) const = 0;

    virtual File::Ptr uploadStickerFile_impl(
        std::int64_t userId, InputFile::Ptr sticker,
        const std::string& stickerFormat) const = 0;

    // Edit a sent message
    virtual Message::Ptr editMessage_impl(
        const Message::Ptr& message, const std::string& newText,
        const TgBot::InlineKeyboardMarkup::Ptr& markup) const = 0;

    // Prefer editMessage_impl for editing text and keyboard
    virtual Message::Ptr editMessageMarkup_impl(
        const StringOrMessage& message,
        const GenericReply::Ptr& markup) const = 0;

    virtual MessageId copyMessage_impl(
        ChatId fromChatId, MessageId messageId,
        ReplyParameters::Ptr replyParameters = nullptr) const = 0;

    virtual bool answerCallbackQuery_impl(const std::string& callbackQueryId,
                                          const std::string& text = "",
                                          bool showAlert = false) const = 0;

    // Delete a sent message
    virtual void deleteMessage_impl(const Message::Ptr& message) const = 0;

    // Delete a range of messages
    virtual void deleteMessages_impl(
        ChatId chatId, const std::vector<MessageId>& messageIds) const = 0;

    // Mute a chat member
    virtual void restrictChatMember_impl(
        ChatId chatId, UserId userId, TgBot::ChatPermissions::Ptr permissions,
        std::uint32_t untilDate) const = 0;

    // Send a file to the chat
    virtual Message::Ptr sendDocument_impl(
        ChatId chatId, FileOrString document, const std::string& caption,
        ReplyParameters::Ptr replyParameters = nullptr,
        GenericReply::Ptr replyMarkup = nullptr,
        const std::string& parseMode = "") const = 0;

    // Send a photo to the chat
    virtual Message::Ptr sendPhoto_impl(
        ChatId chatId, FileOrString photo, const std::string& caption,
        ReplyParameters::Ptr replyParameters = nullptr,
        GenericReply::Ptr replyMarkup = nullptr,
        const std::string& parseMode = "") const = 0;

    // Send a video to the chat
    virtual Message::Ptr sendVideo_impl(
        ChatId chatId, FileOrString photo, const std::string& caption,
        ReplyParameters::Ptr replyParameters = nullptr,
        GenericReply::Ptr replyMarkup = nullptr,
        const std::string& parseMode = "") const = 0;

    virtual Message::Ptr sendDice_impl(const ChatId chatId) const = 0;

    virtual StickerSet::Ptr getStickerSet_impl(
        const std::string& setName) const = 0;

    virtual bool downloadFile_impl(const std::filesystem::path& destfilename,
                                   const std::string& fileid) const = 0;

    virtual User::Ptr getBotUser_impl() const = 0;

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
    static consteval const char* parseModeToStr() {
        switch (mode) {
            case ParseMode::Markdown:
                return "Markdown";
            case ParseMode::HTML:
                return "HTML";
            case ParseMode::None:
                return "";
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
        const Message::Ptr& replyToMessage, const std::string& message,
        const GenericReply::Ptr& replyMarkup = nullptr) const {
        return sendReplyMessage<mode>(replyToMessage->chat->id,
                                      replyToMessage->messageId, message,
                                      replyMarkup);
    }

    template <ParseMode mode = ParseMode::None>
    Message::Ptr sendReplyMessage(
        const ChatId chatId, const MessageId messageId,
        const std::string& message,
        const GenericReply::Ptr& replyMarkup = nullptr) const {
        auto params = std::make_shared<ReplyParameters>();
        params->messageId = messageId;
        params->chatId = chatId;
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
    Message::Ptr sendMessage(const ChatIds& chatId, const std::string& message,
                             const GenericReply::Ptr& markup = nullptr) const {
        return sendMessage_impl(chatId, message, nullptr, markup,
                                parseModeToStr<mode>());
    }

    template <ParseMode mode = ParseMode::None>
    Message::Ptr sendReplyAnimation(const Message::Ptr& replyToMessage,
                                    const FileOrMedia& mediaId,
                                    const std::string& caption = "") const {
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
                               const std::string& caption = "") const {
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

    inline Message::Ptr editMessage(
        const Message::Ptr& message, const std::string& newText,
        const TgBot::InlineKeyboardMarkup::Ptr& markup = nullptr) const {
        return editMessage_impl(message, newText, markup);
    }

    inline Message::Ptr editMessageMarkup(
        const StringOrMessage& message,
        const GenericReply::Ptr& replyMarkup) const {
        return editMessageMarkup_impl(message, replyMarkup);
    }

    inline MessageId copyMessage(
        ChatId fromChatId, MessageId messageId,
        ReplyParameters::Ptr replyParameters = nullptr) const {
        return copyMessage_impl(fromChatId, messageId,
                                std::move(replyParameters));
    }

    inline bool answerCallbackQuery(const std::string& callbackQueryId,
                                    const std::string& text = "",
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
                              const std::string& caption = "",
                              ReplyParameters::Ptr replyParameters = nullptr,
                              GenericReply::Ptr replyMarkup = nullptr) const {
        return sendDocument_impl(chatId, ToFileOrString(document), caption,
                                 std::move(replyParameters),
                                 std::move(replyMarkup),
                                 parseModeToStr<mode>());
    }

    template <ParseMode mode = ParseMode::None>
    Message::Ptr sendPhoto(ChatIds chatId, const FileOrMedia& photo,
                           const std::string& caption = "",
                           ReplyParameters::Ptr replyParameters = nullptr,
                           GenericReply::Ptr replyMarkup = nullptr) const {
        return sendPhoto_impl(chatId, ToFileOrString(photo), caption,
                              std::move(replyParameters),
                              std::move(replyMarkup), parseModeToStr<mode>());
    }

    template <ParseMode mode = ParseMode::None>
    Message::Ptr sendVideo(ChatIds chatId, const FileOrMedia& video,
                           const std::string& caption = "",
                           ReplyParameters::Ptr replyParameters = nullptr,
                           GenericReply::Ptr replyMarkup = nullptr) const {
        return sendVideo_impl(chatId, ToFileOrString(video), caption,
                              std::move(replyParameters),
                              std::move(replyMarkup), parseModeToStr<mode>());
    }

    template <ParseMode mode = ParseMode::None>
    Message::Ptr sendReplyPhoto(const Message::Ptr& replyToMessage,
                                const FileOrMedia& photo,
                                const std::string& caption = "") const {
        return sendPhoto<mode>(replyToMessage->chat->id, photo, caption);
    }

    inline bool downloadFile(const std::filesystem::path& path,
                             const std::string& fileid) const {
        return downloadFile_impl(path, fileid);
    }

    inline User::Ptr getBotUser() const { return getBotUser_impl(); }

    inline Message::Ptr sendDice(const ChatId chat) const {
        return sendDice_impl(chat);
    }

    inline StickerSet::Ptr getStickerSet(const std::string& setName) const {
        return getStickerSet_impl(setName);
    }

    inline bool createNewStickerSet(
        std::int64_t userId, const std::string& name, const std::string& title,
        const std::vector<InputSticker::Ptr>& stickers,
        Sticker::Type stickerType) const {
        return createNewStickerSet_impl(userId, name, title, stickers,
                                        stickerType);
    }

    inline File::Ptr uploadStickerFile(std::int64_t userId,
                                       InputFile::Ptr sticker,
                                       const std::string& stickerFormat) const {
        return uploadStickerFile_impl(userId, std::move(sticker),
                                      stickerFormat);
    }

    // TODO: Any better way than this?
    virtual std::string getCommandModulesStr() const { return {}; }

    virtual bool unloadCommand(const std::string& command) {
        return false;  // Dummy implementation
    }
    virtual bool reloadCommand(const std::string& command) {
        return false;  // Dummy implementation
    }

    virtual void registerCallback(const onanymsg_callback_type& callback) {
        // Dummy implementation
    }
    virtual void registerCallback(const onanymsg_callback_type& callback,
                                  const size_t token) {
        // Dummy implementation
    }
    virtual bool unregisterCallback(const size_t token) {
        return false;  // Dummy implementation
    }

    virtual void onCallbackQuery(
        const TgBot::EventBroadcaster::CallbackQueryListener& listener) {
        // Dummy implementation
    }

   protected:
    static ReplyParameters::Ptr createReplyParametersForReply(
        const Message::Ptr& message) {
        auto ptr = std::make_shared<ReplyParameters>();
        ptr->messageId = message->messageId;
        ptr->chatId = message->chat->id;
        return ptr;
    }
};

// A class to effectively wrap TgBot::Api to stable interface
// This class owns the Bot instance, and users of this code cannot directly
// access it.
class TgBotPPImpl_shared_deps_API TgBotWrapper
    : public InstanceClassBase<TgBotWrapper>,
      public TgBotApi,
      public std::enable_shared_from_this<TgBotWrapper> {
   public:
    // Constructor requires a bot token to create a Bot instance.
    explicit TgBotWrapper(const std::string& token) : _bot(token){};

   private:
    Message::Ptr sendMessage_impl(ChatId chatId, const std::string& text,
                                  ReplyParameters::Ptr replyParameters,
                                  GenericReply::Ptr replyMarkup,
                                  const std::string& parseMode) const override {
        return getApi().sendMessage(chatId, text, nullptr, replyParameters,
                                    replyMarkup, parseMode);
    }

    Message::Ptr sendAnimation_impl(
        ChatId chatId, boost::variant<InputFile::Ptr, std::string> animation,
        const std::string& caption,
        ReplyParameters::Ptr replyParameters = nullptr,
        GenericReply::Ptr replyMarkup = nullptr,
        const std::string& parseMode = "") const override {
        return getApi().sendAnimation(chatId, animation, 0, 0, 0, "", caption,
                                      replyParameters, replyMarkup, parseMode);
    }

    Message::Ptr sendSticker_impl(
        ChatId chatId, boost::variant<InputFile::Ptr, std::string> sticker,
        ReplyParameters::Ptr replyParameters) const override {
        return getApi().sendSticker(chatId, sticker, replyParameters);
    }

    Message::Ptr editMessage_impl(
        const Message::Ptr& message, const std::string& newText,
        const TgBot::InlineKeyboardMarkup::Ptr& markup) const override {
        return getApi().editMessageText(newText, message->chat->id,
                                        message->messageId, "", "", nullptr,
                                        markup);
    }

    Message::Ptr editMessageMarkup_impl(
        const StringOrMessage& message,
        const GenericReply::Ptr& markup) const override {
        return std::visit(
            [=, this](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, Message::Ptr>) {
                    return getApi().editMessageReplyMarkup(
                        arg->chat->id, arg->messageId, "", markup);
                } else if constexpr (std::is_same_v<T, std::string>) {
                    return getApi().editMessageReplyMarkup(0, 0, arg, markup);
                }
                return Message::Ptr();
            },
            message);
    }

    // Copy a message
    MessageId copyMessage_impl(
        ChatId fromChatId, MessageId messageId,
        ReplyParameters::Ptr replyParameters = nullptr) const override {
        const auto ret =
            getApi().copyMessage(fromChatId, fromChatId, messageId, "", "", {},
                                 false, replyParameters);
        if (ret) {
            return ret->messageId;
        }
        return 0;
    }

    bool answerCallbackQuery_impl(const std::string& callbackQueryId,
                                  const std::string& text = "",
                                  bool showAlert = false) const override {
        return getApi().answerCallbackQuery(callbackQueryId, text, showAlert);
    }
    // Delete a sent message
    void deleteMessage_impl(const Message::Ptr& message) const override {
        getApi().deleteMessage(message->chat->id, message->messageId);
    }

    // Delete a range of messages
    void deleteMessages_impl(
        ChatId chatId,
        const std::vector<MessageId>& messageIds) const override {
        getApi().deleteMessages(chatId, messageIds);
    }

    // Mute a chat member
    void restrictChatMember_impl(ChatId chatId, UserId userId,
                                 TgBot::ChatPermissions::Ptr permissions,
                                 std::uint32_t untilDate) const override {
        getApi().restrictChatMember(chatId, userId, permissions, untilDate);
    }

    // Send a file to the chat
    Message::Ptr sendDocument_impl(
        ChatId chatId, FileOrString document, const std::string& caption,
        ReplyParameters::Ptr replyParameters = nullptr,
        GenericReply::Ptr replyMarkup = nullptr,
        const std::string& parseMode = "") const override {
        return getApi().sendDocument(chatId, std::move(document), "", caption,
                                     replyParameters, replyMarkup, parseMode);
    }

    // Send a photo to the chat
    Message::Ptr sendPhoto_impl(
        ChatId chatId, FileOrString photo, const std::string& caption,
        ReplyParameters::Ptr replyParameters = nullptr,
        GenericReply::Ptr replyMarkup = nullptr,
        const std::string& parseMode = "") const override {
        return getApi().sendPhoto(chatId, photo, caption, replyParameters,
                                  replyMarkup, parseMode);
    }

    // Send a video to the chat
    Message::Ptr sendVideo_impl(
        ChatId chatId, FileOrString video, const std::string& caption,
        ReplyParameters::Ptr replyParameters = nullptr,
        GenericReply::Ptr replyMarkup = nullptr,
        const std::string& parseMode = "") const override {
        return getApi().sendVideo(chatId, video, false, 0, 0, 0, "", caption,
                                  replyParameters, replyMarkup, parseMode);
    }

    Message::Ptr sendDice_impl(ChatId chatId) const override {
        static const std::vector<std::string> dices = {"🎲", "🎯", "🏀",
                                                       "⚽", "🎳", "🎰"};

        return getApi().sendDice(
            chatId, false, nullptr, nullptr,
            dices[Random::getInstance()->generate(dices.size() - 1)]);
    }

    StickerSet::Ptr getStickerSet_impl(
        const std::string& setName) const override {
        return getApi().getStickerSet(setName);
    }

    bool createNewStickerSet_impl(
        std::int64_t userId, const std::string& name, const std::string& title,
        const std::vector<InputSticker::Ptr>& stickers,
        Sticker::Type stickerType) const override {
        return getApi().createNewStickerSet(userId, name, title, stickers,
                                            stickerType);
    }

    File::Ptr uploadStickerFile_impl(
        std::int64_t userId, InputFile::Ptr sticker,
        const std::string& stickerFormat) const override {
        return getApi().uploadStickerFile(userId, sticker, stickerFormat);
    }

    bool downloadFile_impl(const std::filesystem::path& destfilename,
                           const std::string& fileid) const override {
        const auto file = getApi().getFile(fileid);
        if (!file) {
            LOG(INFO) << "File " << fileid << " not found in Telegram servers.";
            return false;
        }
        // Download the file
        std::string buffer = getApi().downloadFile(file->filePath);
        // Save the file to a file on disk
        std::fstream ofs(destfilename, std::ios::binary | std::ios::out);
        if (!ofs.is_open()) {
            LOG(ERROR) << "Failed to open file for writing: " << destfilename;
            return false;
        }
        ofs.write(buffer.data(), buffer.size());
        ofs.close();
        return true;
    }

    /**
     * @brief Retrieves the bot's user object.
     *
     * This function retrieves the user object associated with the bot. The
     * user object contains information about the bot's account, such as its
     * username, first name, and last name.
     *
     * @return A shared pointer to the bot's user object.
     */
    User::Ptr getBotUser_impl() const override { return getApi().getMe(); }

   public:
    // Add commands/Remove commands
    void addCommand(const CommandModule& module, bool isReload = false);
    // Remove a command from being handled
    void removeCommand(const std::string& cmd);

    void setDescriptions(const std::string& description,
                         const std::string& shortDescription) {
        getApi().setMyDescription(description);
        getApi().setMyShortDescription(shortDescription);
    }

    [[nodiscard]] bool setBotCommands() const;

    [[nodiscard]] std::string getCommandModulesStr() const override;

    void startPoll();

    bool unloadCommand(const std::string& command) override;
    bool reloadCommand(const std::string& command) override;
    bool isLoadedCommand(const std::string& command);
    bool isKnownCommand(const std::string& command);
    void commandHandler(const command_callback_t& module_callback,
                        unsigned int authflags, MessagePtr message);

    template <unsigned Len>
    static consteval auto getInitCallNameForClient(const char (&str)[Len]) {
        return StringConcat::cat("Register onAnyMessage callbacks: ", str);
    }

    /**
     * @brief Registers a callback function to be called when any message is
     * received.
     *
     * @param callback The function to be called when any message is received.
     */
    void registerCallback(const onanymsg_callback_type& callback) override {
        callbacks.emplace_back(callback);
    }

    /**
     * @brief Registers a callback function with a token to be called when any
     * message is received.
     *
     * @param callback The function to be called when any message is received.
     * @param token A unique identifier for the callback.
     */
    void registerCallback(const onanymsg_callback_type& callback,
                          const size_t token) override {
        callbacksWithToken[token] = callback;
    }

    /**
     * @brief Unregisters a callback function with a token from the list of
     * callbacks to be called when any message is received.
     *
     * @param token The unique identifier of the callback to be unregistered.
     *
     * @return True if the callback with the specified token was found and
     * successfully unregistered, false otherwise.
     */
    bool unregisterCallback(const size_t token) override {
        auto it = callbacksWithToken.find(token);
        if (it == callbacksWithToken.end()) {
            return false;
        }
        callbacksWithToken.erase(it);
        return true;
    }

    void registerOnAnyMsgCallback() {
        getEvents().onAnyMessage([this](const Message::Ptr& message) {
            for (auto& callback : callbacks) {
                callback(shared_from_this(), message);
            }
            for (auto& [token, callback] : callbacksWithToken) {
                callback(shared_from_this(), message);
            }
        });
    }

    void onCallbackQuery(const TgBot::EventBroadcaster::CallbackQueryListener&
                             listener) override {
        getEvents().onCallbackQuery(listener);
    }

   private:
    [[nodiscard]] EventBroadcaster& getEvents() { return _bot.getEvents(); }
    [[nodiscard]] const Api& getApi() const { return _bot.getApi(); }

    std::vector<CommandModule> _modules;
    Bot _bot;
    std::vector<onanymsg_callback_type> callbacks;
    std::map<size_t, onanymsg_callback_type> callbacksWithToken;
    decltype(_modules)::iterator findModulePosition(const std::string& command);
};

// Simple wrapper for TgBot::Message::Ptr.
// Provides easy access, without bot
struct MessageWrapperLimited {
    TgBot::Message::Ptr message;
    std::shared_ptr<MessageWrapperLimited> parent;

    [[nodiscard]] ChatId getChatId() const { return message->chat->id; }
    [[nodiscard]] bool hasReplyToMessage() const {
        return message->replyToMessage != nullptr;
    }
    virtual bool switchToReplyToMessage() noexcept {
        if (message->replyToMessage) {
            parent = std::make_shared<MessageWrapperLimited>(
                MessageWrapperLimited(message, parent));
            message = message->replyToMessage;
            return true;
        }
        return false;
    }
    virtual void switchToParent() noexcept {
        if (parent) {
            message = parent->message;
            parent = parent->parent;
        }
    }

    [[nodiscard]] bool hasExtraText() const noexcept {
        return firstBlank(message) != std::string::npos;
    }
    [[nodiscard]] std::string getExtraText() const noexcept {
        if (!hasExtraText()) {
            return {};
        }
        return message->text.substr(firstBlank(message) + 1);
    }
    void getExtraText(std::string& extraText) const noexcept {
        extraText = getExtraText();
    }
    [[nodiscard]] bool hasSticker() const noexcept {
        return message->sticker != nullptr;
    }
    [[nodiscard]] TgBot::Sticker::Ptr getSticker() const noexcept {
        return message->sticker;
    }
    [[nodiscard]] bool hasAnimation() const noexcept {
        return message->animation != nullptr;
    }
    [[nodiscard]] TgBot::Animation::Ptr getAnimation() const noexcept {
        return message->animation;
    }
    [[nodiscard]] bool hasText() const noexcept {
        return !message->text.empty();
    }
    [[nodiscard]] std::string getText() const noexcept { return message->text; }

    [[nodiscard]] bool hasUser() const noexcept {
        return message->from != nullptr;
    }
    [[nodiscard]] TgBot::User::Ptr getUser() const noexcept {
        return message->from;
    }
    [[nodiscard]] bool hasPhoto() const noexcept {
        return !message->photo.empty();
    }
    [[nodiscard]] std::vector<TgBot::PhotoSize::Ptr> getPhoto() const noexcept {
        return message->photo;
    }
    [[nodiscard]] Message::Ptr getMessage() const noexcept { return message; }
    explicit MessageWrapperLimited(Message::Ptr message)
        : message(std::move(message)) {}

    virtual ~MessageWrapperLimited() = default;

   private:
    MessageWrapperLimited(Message::Ptr message,
                          std::shared_ptr<MessageWrapperLimited> parent)
        : MessageWrapperLimited(std::move(message)) {
        this->parent = std::move(parent);
    }
    [[nodiscard]] static std::string::size_type firstBlank(
        const TgBot::Message::Ptr& msg) noexcept {
        return msg->text.find_first_of(" \n\r");
    }
};

// Simple wrapper for TgBot::Message::Ptr.
// Provides easy access
struct MessageWrapper : MessageWrapperLimited {
    std::shared_ptr<MessageWrapper> parent;
    std::shared_ptr<TgBotApi> botWrapper;  // To access bot APIs

    bool switchToReplyToMessage(const std::string& text) noexcept {
        if (switchToReplyToMessage()) {
            return true;
        } else {
            botWrapper->sendReplyMessage(message, text);
            return false;
        }
    }
    bool switchToReplyToMessage() noexcept override {
        if (message->replyToMessage) {
            parent = std::make_shared<MessageWrapper>(
                MessageWrapper(botWrapper, message, parent));
            message = message->replyToMessage;
            return true;
        }
        return false;
    }
    void switchToParent() noexcept override {
        if (parent) {
            message = parent->message;
            parent = parent->parent;
        }
    }

    explicit MessageWrapper(std::shared_ptr<TgBotApi> api, Message::Ptr message)
        : MessageWrapperLimited(std::move(message)),
          botWrapper(std::move(api)) {}
    ~MessageWrapper() noexcept override {
        Message::Ptr Rmessage;
        if (parent) {
            Rmessage = parent->message;
        } else {
            Rmessage = message;
        }
        if (onExitMessage && !onExitMessage->empty()) {
            botWrapper->sendReplyMessage(Rmessage, *onExitMessage);
        }
    }
    void sendMessageOnExit(std::string message) noexcept {
        onExitMessage = std::move(message);
    }
    void sendMessageOnExit() noexcept { onExitMessage = std::nullopt; }

   private:
    MessageWrapper(std::shared_ptr<TgBotApi> api, Message::Ptr message,
                   std::shared_ptr<MessageWrapper> parent)
        : MessageWrapper(std::move(api), std::move(message)) {
        this->parent = std::move(parent);
    }
    std::optional<std::string> onExitMessage;
};
