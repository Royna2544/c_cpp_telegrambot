#pragma once

#include <Authorization.h>
#include <TgBotPPImplExports.h>
#include <tgbot/tgbot.h>

#include <boost/algorithm/string/trim.hpp>
#include <map>
#include <memory>
#include <optional>
#include <utility>

#include "CompileTimeStringConcat.hpp"
#include "InstanceClassBase.hpp"
#include "Types.h"

using TgBot::Api;
using TgBot::Bot;
using TgBot::BotCommand;
using TgBot::Chat;
using TgBot::EventBroadcaster;
using TgBot::GenericReply;
using TgBot::Message;
using TgBot::ReplyParameters;
using TgBot::User;

struct MessageWrapper;
struct MessageWrapperLimited;

using MessagePtr = const Message::Ptr&;

class TgBotWrapper;
struct CommandModule;

#define DYN_COMMAND_SYM_STR "loadcmd"
#define DYN_COMMAND_SYM loadcmd
#define DYN_COMMAND_FN(n, m) \
    extern "C" bool DYN_COMMAND_SYM(const char* n, CommandModule& m)

using TgBot::Message;
using command_callback_t =
    std::function<void(TgBotWrapper* wrapper, MessagePtr)>;
using onanymsg_callback_type =
    std::function<void(TgBotWrapper*, const Message::Ptr&)>;

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
struct TgBotPPImpl_API MediaIds {
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

// A class to effectively wrap TgBot::Api to stable interface
// This class owns the Bot instance, and users of this code cannot directly
// access it.
class TgBotPPImpl_API TgBotWrapper : public InstanceClassBase<TgBotWrapper> {
   public:
    // Constructor requires a bot token to create a Bot instance.
    explicit TgBotWrapper(const std::string& token) : _bot(token){};

    // Add commands/Remove commands
    void addCommand(const CommandModule& module, bool isReload = false);
    // Remove a command from being handled
    void removeCommand(const std::string& cmd);

    // Obtain MessageWrapper object
    static MessageWrapper getMessageWrapper(const Message::Ptr& msg);
    static MessageWrapperLimited getMessageWrapperLimited(
        const Message::Ptr& msg);

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

    // Call TgBot::Api methods through this wrapper.

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
        return getApi().sendMessage(
            replyToMessage->chat->id, message, nullptr,
            createReplyParametersForReply(replyToMessage), replyMarkup,
            parseModeToStr<mode>());
    }

    /**
     * @brief Sends a message to the specified chat reference message's chat.
     *
     * This function uses the TgBot::Api::sendMessage method to send a message
     * to the chat where the specified `chatReferenceMessage` was sent. The
     * function takes the chat reference message's chat ID and the text content
     * of the message to be sent.
     *
     * @param chatReferenceMessage The message that serves as a reference for
     * the chat where the new message will be sent.
     * @param message The text content of the message to be sent.
     *
     * @return A shared pointer to the sent message.
     */
    Message::Ptr sendMessage(const Message::Ptr& chatReferenceMessage,
                             const std::string& message) const {
        return sendMessage(chatReferenceMessage->chat->id, message);
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
    Message::Ptr sendMessage(const ChatId& chatId,
                             const std::string& message) const {
        return getApi().sendMessage(chatId, message);
    }

    template <ParseMode mode = ParseMode::None>
    Message::Ptr sendReplyAnimation(const Message::Ptr& replyToMessage,
                                    const MediaIds& mediaId,
                                    const std::string& caption = "") const {
        return getApi().sendAnimation(
            replyToMessage->chat->id, mediaId.id, 0, 0, 0, "", caption,
            createReplyParametersForReply(replyToMessage), nullptr,
            parseModeToStr<mode>());
    }

    Message::Ptr sendReplySticker(const Message::Ptr& replyToMessage,
                                  const MediaIds& mediaId) const {
        return getApi().sendSticker(
            replyToMessage->chat->id, mediaId.id,
            createReplyParametersForReply(replyToMessage));
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
    Message::Ptr sendAnimation(const Message::Ptr& replyToMessage,
                               const MediaIds& mediaId,
                               const std::string& caption = "") const {
        return sendAnimation<mode>(replyToMessage->chat->id, mediaId, caption);
    }

    /**
     * @brief Sends an animation message to the specified chat.
     *
     * This function uses the TgBot::Api::sendAnimation method to send an
     * animation message to the chat specified by the given `chatId`. The
     * function takes the chat ID and the media ID of the animation to be sent.
     *
     * @param chatId The ID of the chat to which the animation message will be
     * sent.
     * @param mediaId The ID of the animation to be sent.
     * @param caption An optional caption for the animation message.
     *
     * @return A shared pointer to the sent animation message.
     */
    template <ParseMode mode = ParseMode::None>
    Message::Ptr sendAnimation(const ChatId& chatId, const MediaIds& mediaId,
                               const std::string& caption = "") const {
        return getApi().sendAnimation(chatId, mediaId.id, 0, 0, 0, "", caption,
                                      nullptr, nullptr, parseModeToStr<mode>());
    }

    /**
     * @brief Sends a sticker message to the specified chat reference message's
     * chat.
     *
     * This function uses the TgBot::Api::sendSticker method to send a sticker
     * message to the chat where the specified `chatReferenceMessage` was sent.
     * The function takes the chat reference message's chat ID and the media ID
     * of the sticker to be sent.
     *
     * @param chatReferenceMessage The message that serves as a reference for
     * the chat where the new sticker message will be sent.
     * @param mediaId The ID of the sticker to be sent.
     *
     * @return A shared pointer to the sent sticker message.
     */
    Message::Ptr sendSticker(const Message::Ptr& chatReferenceMessage,
                             const MediaIds& mediaId) const {
        return sendSticker(chatReferenceMessage->chat->id, mediaId);
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
    Message::Ptr sendSticker(const ChatId chatId,
                             const MediaIds& mediaId) const {
        return getApi().sendSticker(chatId, mediaId.id);
    }
    /**
     * @brief Edits the text of a message.
     *
     * This function edits the text of a message that was previously sent by the
     * bot. The function takes the message object, the new text content, and the
     * chat ID of the message. It returns a shared pointer to the edited
     * message.
     *
     * @param message The message object whose text is to be edited.
     * @param newText The new text content for the message.
     * @param chatId The ID of the chat where the message was sent.
     *
     * @return A shared pointer to the edited message.
     */
    Message::Ptr editMessage(const Message::Ptr& message,
                             const std::string& newText) const {
        return getApi().editMessageText(newText, message->chat->id,
                                        message->messageId);
    }

    void deleteMessage(const Message::Ptr& message) const {
        getApi().deleteMessage(message->chat->id, message->messageId);
    }

    /**
     * @brief Retrieves the bot's user object.
     *
     * This function retrieves the user object associated with the bot. The user
     * object contains information about the bot's account, such as its
     * username, first name, and last name.
     *
     * @return A shared pointer to the bot's user object.
     */
    [[nodiscard]] User::Ptr getBotUser() const { return _bot.getApi().getMe(); }

    [[nodiscard]] bool setBotCommands() const;

    [[nodiscard]] std::string getCommandModulesStr() const;

    // TODO: Remove
    Bot& getBot() { return _bot; }

    // TODO: Private it
    [[nodiscard]] const Api& getApi() const { return _bot.getApi(); }

    bool unloadCommand(const std::string& command);
    bool reloadCommand(const std::string& command);
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
    void registerCallback(const onanymsg_callback_type& callback) {
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
                          const size_t token) {
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
    bool unregisterCallback(const size_t token) {
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
                callback(this, message);
            }
            for (auto& [token, callback] : callbacksWithToken) {
                callback(this, message);
            }
        });
    }

   private:
    static ReplyParameters::Ptr createReplyParametersForReply(
        const Message::Ptr& message) {
        auto ptr = std::make_shared<ReplyParameters>();
        ptr->messageId = message->messageId;
        ptr->chatId = message->chat->id;
        return ptr;
    }
    [[nodiscard]] EventBroadcaster& getEvents() { return _bot.getEvents(); }

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
    std::shared_ptr<TgBotWrapper> botWrapper;  // To access bot APIs

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
                MessageWrapper(message, parent));
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

    explicit MessageWrapper(Message::Ptr message)
        : MessageWrapperLimited(std::move(message)),
          botWrapper(TgBotWrapper::getInstance()) {}
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
    MessageWrapper(Message::Ptr message, std::shared_ptr<MessageWrapper> parent)
        : MessageWrapper(std::move(message)) {
        this->parent = std::move(parent);
        botWrapper = TgBotWrapper::getInstance();
    }
    std::optional<std::string> onExitMessage;
};
