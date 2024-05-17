#include <tgbot/types/Message.h>

#include "BotReplyMessage.h"
#include "StringToolsExt.h"
#include <tgbot/Bot.h>
#include <tgbot/types/Sticker.h>

using TgBot::Bot;

// Simple wrapper for TgBot::Message::Ptr.
// Provides easy access
struct MessageWrapper {
    TgBot::Message::Ptr message;

    bool switchToReplyToMessage(const Bot& bot, const std::string& text) noexcept {
        if (message->replyToMessage) {
            message = message->replyToMessage;
            return true;
        } else {
            bot_sendReplyMessage(bot, message, text);
            return false;
        }
    }
    void switchToReplyToMessage() noexcept {
        if (message->replyToMessage) {
            message = message->replyToMessage;
        }
    }
    [[nodiscard]] bool hasExtraText() const noexcept {
        return firstBlank(message) != std::string::npos;
    }
    [[nodiscard]] std::string getExtraText() const noexcept {
        return message->text.substr(firstBlank(message));
    }
    void getExtraText(std::string& extraText) const noexcept {
        extraText = message->text.substr(firstBlank(message));
        while (isEmptyChar(extraText.front())) {
            extraText = extraText.substr(1);
        }
    }
    [[nodiscard]] bool hasSticker() const noexcept {
        return message->sticker != nullptr;
    }
    [[nodiscard]] TgBot::Sticker::Ptr getSticker() const noexcept {
        return message->sticker;
    }

   private:
    using Message = TgBot::Message;
    [[nodiscard]] std::string::size_type firstBlank(
        const TgBot::Message::Ptr& msg) const noexcept {
        return msg->text.find_first_of(" \n\r");
    }
};