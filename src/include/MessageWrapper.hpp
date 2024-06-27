#include <tgbot/Bot.h>
#include <tgbot/types/Message.h>
#include <tgbot/types/Sticker.h>

#include <memory>
#include <optional>
#include <utility>

#include "BotClassBase.h"
#include "BotReplyMessage.h"

using TgBot::Bot;

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
struct MessageWrapper : BotClassBase, MessageWrapperLimited {
    std::shared_ptr<MessageWrapper> parent;

    bool switchToReplyToMessage(const std::string& text) noexcept {
        if (switchToReplyToMessage()) {
            return true;
        } else {
            bot_sendReplyMessage(_bot, message, text);
            return false;
        }
    }
    bool switchToReplyToMessage() noexcept override {
        if (message->replyToMessage) {
            parent = std::make_shared<MessageWrapper>(
                MessageWrapper(_bot, message, parent));
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

    explicit MessageWrapper(const Bot& bot, TgBot::Message::Ptr message)
        : BotClassBase(bot), MessageWrapperLimited(std::move(message)) {}
    ~MessageWrapper() noexcept override {
        Message::Ptr Rmessage;
        if (parent) {
            Rmessage = parent->message;
        } else {
            Rmessage = message;
        }
        if (onExitMessage && !onExitMessage->empty()) {
            bot_sendReplyMessage(_bot, Rmessage, *onExitMessage);
        }
    }
    void sendMessageOnExit(std::string message) noexcept {
        onExitMessage = std::move(message);
    }
    void sendMessageOnExit() noexcept { onExitMessage = std::nullopt; }

   private:
    MessageWrapper(const Bot& bot, Message::Ptr message,
                   std::shared_ptr<MessageWrapper> parent)
        : MessageWrapper(bot, std::move(message)) {
        this->parent = std::move(parent);
    }
    std::optional<std::string> onExitMessage;
};
