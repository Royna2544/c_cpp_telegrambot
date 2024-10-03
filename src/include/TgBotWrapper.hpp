#pragma once

#include <Authorization.h>
#include <TgBotPPImpl_shared_depsExports.h>
#include <Types.h>
#include <absl/log/check.h>
#include <tgbot/tgbot.h>

#include <CompileTimeStringConcat.hpp>
#include <InstanceClassBase.hpp>
#include <ReplyParametersExt.hpp>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <ios>
#include <memory>
#include <mutex>
#include <queue>
#include <sstream>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

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

// API part wrapper (base)
struct TgBotApi;
struct MessageExt;
struct CommandModule;

using MessagePtr = std::shared_ptr<MessageExt>;
using ApiPtr = const std::shared_ptr<TgBotApi>&;

#define DYN_COMMAND_SYM_STR "loadcmd"
#define DYN_COMMAND_SYM loadcmd
#define DYN_COMMAND_FN(n, m) \
    extern "C" bool DYN_COMMAND_SYM(const std::string_view n, CommandModule& m)
#define COMMAND_HANDLER_NAME(cmd) handle_command_##cmd
#define DECLARE_COMMAND_HANDLER(cmd, w, m) \
    void COMMAND_HANDLER_NAME(cmd)(ApiPtr w, MessagePtr m)

using command_callback_t = std::function<void(ApiPtr wrapper, MessagePtr)>;
using loadcmd_function_cstyle_t = bool (*)(const std::string_view,
                                           CommandModule&);
using loadcmd_function_t =
    std::function<std::remove_pointer_t<loadcmd_function_cstyle_t>>;

enum class MessageExt_Attrs {
    IsReplyMessage,
    ExtraText,
    Photo,
    Sticker,
    Animation,
};

// Split type to obtain arguments.
enum class SplitMessageText {
    None,
    ByWhitespace,
    ByComma,
};

template <MessageExt_Attrs T>
struct GetAttribute;
template <>
struct GetAttribute<MessageExt_Attrs::Animation> {
    using type = Animation::Ptr;
};
template <>
struct GetAttribute<MessageExt_Attrs::Sticker> {
    using type = Sticker::Ptr;
};
template <>
struct GetAttribute<MessageExt_Attrs::Photo> {
    using type = TgBot::PhotoSize::Ptr;
};
template <>
struct GetAttribute<MessageExt_Attrs::ExtraText> {
    using type = std::string;
};
template <>
struct GetAttribute<MessageExt_Attrs::IsReplyMessage> {
    using type = std::nullptr_t;
};

struct TgBotPPImpl_shared_deps_API MessageExt
    : Message,
      public std::enable_shared_from_this<MessageExt> {
    using Ptr = std::shared_ptr<MessageExt>;
    struct Command {
        // e.g. start in /start@some_bot
        std::string name;
        // e.g. @some_bot in /start@some_bot
        std::string target;
    };
    MessageExt() = default;
    // Make assignments possible from Message
    MessageExt(const Message& other) : Message(other) { update(); }
    MessageExt(Message&& other) noexcept : Message(std::move(other)) {
        update();
    }
    MessageExt(const Message::Ptr& other) : Message(*other) { update(); }

    // Update the Ext parameters with the base.
    void update();
    // Update the Ext parameters with the base and howto split the text.
    void update(const SplitMessageText& how);

    using Attrs = ::MessageExt_Attrs;

    // Get an attribute value
    template <Attrs attr>
    [[nodiscard]] auto get() const {
        return get_attribute<attr>(shared_from_this());
    }

    template <Attrs attr>
    [[nodiscard]] typename GetAttribute<attr>::type replyToMessage_get() const {
        if (!replyToMessage) {
            return {};
        }
        return get_attribute<attr>(
            std::make_shared<MessageExt>(replyToMessage));
    }

    // Has an attribute?
    template <Attrs... attrs>
    [[nodiscard]] bool has() const {
        return (has_attribute(shared_from_this(), attrs) && ...);
    }
    template <Attrs... attrs>
    [[nodiscard]] bool replyToMessage_has() const {
        if (!replyToMessage) {
            return false;
        }
        return (has_attribute(std::make_shared<MessageExt>(replyToMessage),
                              attrs) &&
                ...);
    }

    template <Attrs... attrs>
    std::string why_not() {
        std::stringstream message;
        ([this, &message](const Attrs& attr) {
            message << "Has ";
            switch (attr) {
                case Attrs::IsReplyMessage:
                    message << "reply to message";
                    break;
                case Attrs::ExtraText:
                    message << "arguments";
                    break;
                case Attrs::Photo:
                    message << "photo";
                    break;
                case Attrs::Sticker:
                    message << "sticker";
                    break;
                case Attrs::Animation:
                    message << "animation";
                    break;
            }
            message << "? " << std::boolalpha
                    << has_attribute(shared_from_this(), attr) << std::endl;
        }(attrs...));
        return message.str();
    }

    [[nodiscard]] std::vector<std::string> arguments() const {
        return _arguments;
    }
    [[nodiscard]] const Command& get_command() const { return command; }

#define HAS_AND_GETTER(name, varname)                     \
    bool has##name() const { return varname != nullptr; } \
    name ::Ptr get##name() const { return varname; }

    HAS_AND_GETTER(Chat, chat);
    HAS_AND_GETTER(Message, replyToMessage);
    HAS_AND_GETTER(Sticker, sticker);
    HAS_AND_GETTER(Animation, animation);
    HAS_AND_GETTER(User, from);

   private:
    // Like /start@some_bot
    Command command;
    // Additional arguments following the bot command
    std::string _extra_args;
    // Splitted arguments
    std::vector<std::string> _arguments;

    static bool has_attribute(const std::shared_ptr<const MessageExt>& interest,
                              const Attrs attr) {
        switch (attr) {
            case Attrs::IsReplyMessage:
                return interest->replyToMessage != nullptr;
            case Attrs::ExtraText:
                return !interest->_extra_args.empty();
            case Attrs::Photo:
                return !interest->photo.empty();
            case Attrs::Sticker:
                return interest->sticker != nullptr;
            case Attrs::Animation:
                return interest->animation != nullptr;
        }
        return false;
    }
    template <Attrs attr>
        requires(attr != Attrs::IsReplyMessage)
    [[nodiscard]] static constexpr typename GetAttribute<attr>::type
    get_attribute(const std::shared_ptr<const MessageExt>& interest) {
        if constexpr (attr == Attrs::ExtraText) {
            return interest->_extra_args;
        } else if constexpr (attr == Attrs::Photo) {
            return interest->photo.back();
        } else if constexpr (attr == Attrs::Sticker) {
            return interest->sticker;
        } else if constexpr (attr == Attrs::Animation) {
            return interest->animation;
        }
        CHECK(false) << "Unreachable: " << static_cast<int>(attr);
        return {};
    }
};

struct TgBotPPImpl_shared_deps_API CommandModule : TgBot::BotCommand {
    using Ptr = std::unique_ptr<CommandModule>;

    enum Flags { None = 0, Enforced = 1 << 0, HideDescription = 1 << 1 };
    command_callback_t function;
    unsigned int flags = None;

    struct ValidArgs {
        // Is it enabled?
        bool enabled;
        // Int array to the valid argument count array.
        std::vector<int> counts;
        // Split type to obtain arguments.
        using Split = ::SplitMessageText;
        Split split_type;
        // Usage information for the command.
        std::string usage;
    } valid_arguments{};

   private:
    bool isLoaded = false;
    // dlclose RAII handle
    std::unique_ptr<void, int (*)(void*)> handle;
    std::filesystem::path filePath;

   public:
    explicit CommandModule(std::filesystem::path filePath);
    ~CommandModule() override = default;

    bool load();
    bool unload();
    [[nodiscard]] bool getLoaded() const { return isLoaded; }

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
    ChatIds(MessagePtr message) : _id(message->chat->id) {}
    ChatIds(Message::Ptr message) : _id(message->chat->id) {}
    operator ChatId() const { return _id; }
    ChatId _id;
};

// Base interface for operations involving TgBot...
struct TgBotApi {
   public:
    TgBotApi() = default;
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
        ChatId chatId, const std::string& text,
        ReplyParametersExt::Ptr replyParameters = nullptr,
        GenericReply::Ptr replyMarkup = nullptr,
        const std::string& parseMode = "Markdown") const = 0;

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
        ChatId chatId, FileOrString animation, const std::string& caption = "",
        ReplyParametersExt::Ptr replyParameters = nullptr,
        GenericReply::Ptr replyMarkup = nullptr,
        const std::string& parseMode = "") const = 0;

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
        std::int64_t userId, const std::string& name, const std::string& title,
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
        const std::string& stickerFormat) const = 0;

    /**
     * @brief Edits a sent message.
     *
     * This function edits a sent message with the given parameters.
     *
     * @param message The pointer to the message to be edited.
     * @param newText The new text for the message.
     * @param markup The new inline keyboard markup for the message.
     *
     * @return A shared pointer to the edited message.
     */
    virtual Message::Ptr editMessage_impl(
        const Message::Ptr& message, const std::string& newText,
        const TgBot::InlineKeyboardMarkup::Ptr& markup) const = 0;

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
    virtual bool answerCallbackQuery_impl(const std::string& callbackQueryId,
                                          const std::string& text = "",
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
        ChatId chatId, FileOrString document, const std::string& caption = "",
        ReplyParametersExt::Ptr replyParameters = nullptr,
        GenericReply::Ptr replyMarkup = nullptr,
        const std::string& parseMode = "") const = 0;

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
        ChatId chatId, FileOrString photo, const std::string& caption = "",
        ReplyParametersExt::Ptr replyParameters = nullptr,
        GenericReply::Ptr replyMarkup = nullptr,
        const std::string& parseMode = "") const = 0;

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
        ChatId chatId, FileOrString video, const std::string& caption = "",
        ReplyParametersExt::Ptr replyParameters = nullptr,
        GenericReply::Ptr replyMarkup = nullptr,
        const std::string& parseMode = "") const = 0;

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
        const std::string& setName) const = 0;

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
                                   const std::string& fileId) const = 0;

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
        const MessageThreadId messageTid, const std::string& message,
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

    inline MessageId copyMessage(const Message::Ptr& message) const {
        return copyMessage_impl(message->chat->id, message->messageId);
    }

    inline MessageId copyAndReplyAsMessage(const Message::Ptr& message) const {
        return copyMessage_impl(message->chat->id, message->messageId,
                                createReplyParametersForReply(message));
    }

    [[nodiscard]] inline bool answerCallbackQuery(
        const std::string& callbackQueryId, const std::string& text = "",
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
                              ReplyParametersExt::Ptr replyParameters = nullptr,
                              GenericReply::Ptr replyMarkup = nullptr) const {
        return sendDocument_impl(chatId, ToFileOrString(document), caption,
                                 std::move(replyParameters),
                                 std::move(replyMarkup),
                                 parseModeToStr<mode>());
    }

    template <ParseMode mode = ParseMode::None>
    Message::Ptr sendPhoto(ChatIds chatId, const FileOrMedia& photo,
                           const std::string& caption = "",
                           ReplyParametersExt::Ptr replyParameters = nullptr,
                           GenericReply::Ptr replyMarkup = nullptr) const {
        return sendPhoto_impl(chatId, ToFileOrString(photo), caption,
                              std::move(replyParameters),
                              std::move(replyMarkup), parseModeToStr<mode>());
    }

    template <ParseMode mode = ParseMode::None>
    Message::Ptr sendVideo(ChatIds chatId, const FileOrMedia& video,
                           const std::string& caption = "",
                           ReplyParametersExt::Ptr replyParameters = nullptr,
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

    [[nodiscard]] inline bool downloadFile(const std::filesystem::path& path,
                                           const std::string& fileid) const {
        return downloadFile_impl(path, fileid);
    }

    [[nodiscard]] inline User::Ptr getBotUser() const {
        return getBotUser_impl();
    }

    inline Message::Ptr sendDice(const ChatId chat) const {
        return sendDice_impl(chat);
    }

    [[nodiscard]] inline StickerSet::Ptr getStickerSet(
        const std::string& setName) const {
        return getStickerSet_impl(setName);
    }

    [[nodiscard]] inline bool createNewStickerSet(
        std::int64_t userId, const std::string& name, const std::string& title,
        const std::vector<InputSticker::Ptr>& stickers,
        Sticker::Type stickerType) const {
        return createNewStickerSet_impl(userId, name, title, stickers,
                                        stickerType);
    }

    [[nodiscard]] inline File::Ptr uploadStickerFile(
        std::int64_t userId, InputFile::Ptr sticker,
        const std::string& stickerFormat) const {
        return uploadStickerFile_impl(userId, std::move(sticker),
                                      stickerFormat);
    }

    inline bool pinMessage(Message::Ptr message) {
        return pinMessage_impl(message);
    }
    inline bool unpinMessage(Message::Ptr message) {
        return unpinMessage_impl(message);
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

    // TODO: Any better way than this?
    virtual std::string getCommandModulesStr() const { return {}; }

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

    using onAnyMessage_handler_t =
        std::function<AnyMessageResult(ApiPtr, MessagePtr)>;

    virtual void onAnyMessage(const onAnyMessage_handler_t& callback) {
        // Dummy implementation
    }

    virtual void onCallbackQuery(
        const TgBot::EventBroadcaster::CallbackQueryListener& listener) {
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

// A class to effectively wrap TgBot::Api to stable interface
// This class owns the Bot instance, and users of this code cannot directly
// access it.
class TgBotPPImpl_shared_deps_API TgBotWrapper
    : public InstanceClassBase<TgBotWrapper>,
      public TgBotApi,
      public std::enable_shared_from_this<TgBotWrapper> {
   public:
    // Constructor requires a bot token to create a Bot instance.
    explicit TgBotWrapper(const std::string& token);
    ~TgBotWrapper() override;

   private:
    Message::Ptr sendMessage_impl(ChatId chatId, const std::string& text,
                                  ReplyParametersExt::Ptr replyParameters,
                                  GenericReply::Ptr replyMarkup,
                                  const std::string& parseMode) const override;

    Message::Ptr sendAnimation_impl(
        ChatId chatId, boost::variant<InputFile::Ptr, std::string> animation,
        const std::string& caption,
        ReplyParametersExt::Ptr replyParameters = nullptr,
        GenericReply::Ptr replyMarkup = nullptr,
        const std::string& parseMode = "") const override;

    Message::Ptr sendSticker_impl(
        ChatId chatId, boost::variant<InputFile::Ptr, std::string> sticker,
        ReplyParametersExt::Ptr replyParameters) const override;

    Message::Ptr editMessage_impl(
        const Message::Ptr& message, const std::string& newText,
        const TgBot::InlineKeyboardMarkup::Ptr& markup) const override;

    Message::Ptr editMessageMarkup_impl(
        const StringOrMessage& message,
        const GenericReply::Ptr& markup) const override;

    // Copy a message
    MessageId copyMessage_impl(
        ChatId fromChatId, MessageId messageId,
        ReplyParametersExt::Ptr replyParameters = nullptr) const override;

    bool answerCallbackQuery_impl(const std::string& callbackQueryId,
                                  const std::string& text = "",
                                  bool showAlert = false) const override {
        return getApi().answerCallbackQuery(callbackQueryId, text, showAlert);
    }

    void deleteMessage_impl(const Message::Ptr& message) const override;

    // Delete a range of messages
    void deleteMessages_impl(
        ChatId chatId, const std::vector<MessageId>& messageIds) const override;

    // Mute a chat member
    void restrictChatMember_impl(ChatId chatId, UserId userId,
                                 TgBot::ChatPermissions::Ptr permissions,
                                 std::uint32_t untilDate) const override;

    // Send a file to the chat
    Message::Ptr sendDocument_impl(
        ChatId chatId, FileOrString document, const std::string& caption,
        ReplyParametersExt::Ptr replyParameters = nullptr,
        GenericReply::Ptr replyMarkup = nullptr,
        const std::string& parseMode = "") const override;

    // Send a photo to the chat
    Message::Ptr sendPhoto_impl(
        ChatId chatId, FileOrString photo, const std::string& caption,
        ReplyParametersExt::Ptr replyParameters = nullptr,
        GenericReply::Ptr replyMarkup = nullptr,
        const std::string& parseMode = "") const override;

    // Send a video to the chat
    Message::Ptr sendVideo_impl(
        ChatId chatId, FileOrString video, const std::string& caption,
        ReplyParametersExt::Ptr replyParameters = nullptr,
        GenericReply::Ptr replyMarkup = nullptr,
        const std::string& parseMode = "") const override;

    Message::Ptr sendDice_impl(ChatId chatId) const override;

    StickerSet::Ptr getStickerSet_impl(
        const std::string& setName) const override;

    bool createNewStickerSet_impl(
        std::int64_t userId, const std::string& name, const std::string& title,
        const std::vector<InputSticker::Ptr>& stickers,
        Sticker::Type stickerType) const override;

    File::Ptr uploadStickerFile_impl(
        std::int64_t userId, InputFile::Ptr sticker,
        const std::string& stickerFormat) const override;

    bool downloadFile_impl(const std::filesystem::path& destfilename,
                           const std::string& fileid) const override;

    /**
     * @brief Retrieves the bot's user object.
     *
     * This function retrieves the user object associated with the bot. The
     * user object contains information about the bot's account, such as its
     * username, first name, and last name.
     *
     * @return A shared pointer to the bot's user object.
     */
    User::Ptr getBotUser_impl() const override;

    bool pinMessage_impl(Message::Ptr message) const override;
    bool unpinMessage_impl(Message::Ptr message) const override;
    bool banChatMember_impl(const Chat::Ptr& chat,
                            const User::Ptr& user) const override;
    bool unbanChatMember_impl(const Chat::Ptr& chat,
                              const User::Ptr& user) const override;
    User::Ptr getChatMember_impl(ChatId chat, UserId user) const override;

    // A global link preview options
    TgBot::LinkPreviewOptions::Ptr globalLinkOptions;

   public:
    // Add commands/Remove commands
    void addCommand(CommandModule::Ptr module);
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
    void commandHandler(unsigned int authflags, MessageExt::Ptr message);

    template <unsigned Len>
    static consteval auto getInitCallNameForClient(const char (&str)[Len]) {
        return StringConcat::cat("Register onAnyMessage callbacks: ", str);
    }

   private:
    // Protect callbacks
    std::mutex callbackMutex;
    // A callback list where it is called when any message is received
    std::vector<onAnyMessage_handler_t> callbacks;

   public:
    /**
     * @brief Registers a callback function to be called when any message is
     * received.
     *
     * @param callback The function to be called when any message is
     * received.
     */
    void onAnyMessage(const onAnyMessage_handler_t& callback) override {
        const std::lock_guard<std::mutex> _(callbackMutex);
        callbacks.emplace_back(callback);
    }

    void onCallbackQuery(const TgBot::EventBroadcaster::CallbackQueryListener&
                             listener) override {
        getEvents().onCallbackQuery(listener);
    }

   private:
    [[nodiscard]] EventBroadcaster& getEvents() { return _bot.getEvents(); }
    [[nodiscard]] const Api& getApi() const { return _bot.getApi(); }

    // Support async command handling
    // A flag to stop worker
    std::atomic<bool> stopWorker = false;
    // A queue to handle command (commandname, async future)
    std::queue<std::pair<std::string, std::future<void>>> asyncTasks;
    // mutex to protect shared queue
    std::mutex queueMutex;
    // condition variable to wait for async tasks to finish.
    std::condition_variable queueCV;
    // worker thread(s) to consume command queue
    std::vector<std::thread> workerThreads;

    std::vector<std::unique_ptr<CommandModule>> _modules;
    Bot _bot;
    decltype(_modules)::iterator findModulePosition(const std::string& command);
    void onAnyMessageFunction(const Message::Ptr& message);
    void startQueueConsumerThread();
    void stopQueueConsumerThread();
};
