#pragma once

#include <Authorization.h>
#include <tgbot/tgbot.h>

#include <boost/algorithm/string/trim.hpp>
#include <memory>

#include "InstanceClassBase.hpp"
#include "Types.h"
#include "tgbot/types/BotCommand.h"

using TgBot::Api;
using TgBot::Bot;
using TgBot::Chat;
using TgBot::EventBroadcaster;
using TgBot::Message;
using TgBot::ReplyParameters;
using TgBot::User;
using TgBot::BotCommand;

struct MessageWrapper;
struct MessageWrapperLimited;

// A class to effectively wrap TgBot::Api to stable interface
// This class owns the Bot instance, and users of this code cannot directly
// access it.
class TgBotWrapper : public InstanceClassBase<TgBotWrapper> {
   public:
    // Constructor requires a bot token to create a Bot instance.
    explicit TgBotWrapper(const std::string& token) : _bot(token){};
    using command_callback_t = std::function<void(const Message::Ptr&)>;

    // Add commands/Remove commands
    void addCommand(const std::string& cmd, command_callback_t callback,
                    bool enforced);
    // Remove a command from being handled
    void removeCommand(const std::string& cmd);

    // Obtain MessageWrapper object
    static MessageWrapper getMessageWrapper(const Message::Ptr& msg);
    static MessageWrapperLimited getMessageWrapperLimited(
        const Message::Ptr& msg);

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
    Message::Ptr sendReplyMessage(const Message::Ptr& replyToMessage,
                                  const std::string& message) const {
        return getApi().sendMessage(
            replyToMessage->chat->id, message, nullptr,
            createReplyParametersForReply(replyToMessage));
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
        return getApi().sendMessage(chatReferenceMessage->chat->id, message);
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
    Message::Ptr sendMessage(const ChatId& chatId, const std::string& message) {
        return getApi().sendMessage(chatId, message);
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
                             const std::string& newText) {
        return getApi().editMessageText(newText, message->chat->id,
                                        message->messageId);
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
    User::Ptr getBotUser() { return _bot.getApi().getMe(); }

    bool setBotCommands(const std::vector<BotCommand::Ptr>& commands) const;

    // TODO: Remove
    Bot& getBot() {
        return _bot;
    }
   private:
    static ReplyParameters::Ptr createReplyParametersForReply(
        const Message::Ptr& message) {
        auto ptr = std::make_shared<ReplyParameters>();
        ptr->messageId = message->messageId;
        ptr->chatId = message->chat->id;
        return ptr;
    }
    [[nodiscard]] const Api& getApi() const { return _bot.getApi(); }
    [[nodiscard]] EventBroadcaster& getEvents() { return _bot.getEvents(); }
    Bot _bot;
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
