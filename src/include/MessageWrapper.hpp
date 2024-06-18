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
// Provides easy access
struct MessageWrapper : BotClassBase {
    TgBot::Message::Ptr message;
    std::shared_ptr<MessageWrapper> parent;

    [[nodiscard]] ChatId getChatId() const { return message->chat->id; }
    [[nodiscard]] bool hasReplyToMessage() const {
        return message->replyToMessage != nullptr;
    }
    bool switchToReplyToMessage(const Bot& bot,
                                const std::string& text) noexcept {
        if (switchToReplyToMessage()) {
            return true;
        } else {
            bot_sendReplyMessage(bot, message, text);
            return false;
        }
    }
    bool switchToReplyToMessage() noexcept {
        if (message->replyToMessage) {
            parent = std::make_shared<MessageWrapper>(
                MessageWrapper(_bot, message, parent));
            message = message->replyToMessage;
            return true;
        }
        return false;
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
    void sendMessageOnExit(std::string message) noexcept {
        onExitMessage = std::move(message);
    }

    explicit MessageWrapper(const Bot& bot, Message::Ptr message)
        : BotClassBase(bot), message(std::move(message)) {}
    ~MessageWrapper() noexcept {
        Message::Ptr Rmessage;
        if (parent) {
            Rmessage = parent->message;
        } else {
            Rmessage = message;
        }
        if (onExitMessage) {
            bot_sendReplyMessage(_bot, Rmessage, *onExitMessage);
        }
    }

   private:
    using Message = TgBot::Message;

    MessageWrapper(const Bot& bot, Message::Ptr message,
                   std::shared_ptr<MessageWrapper> parent)
        : MessageWrapper(bot, std::move(message)) {
        this->parent = std::move(parent);
    }
    [[nodiscard]] static std::string::size_type firstBlank(
        const TgBot::Message::Ptr& msg) noexcept {
        return msg->text.find_first_of(" \n\r");
    }
    std::optional<std::string> onExitMessage;
};