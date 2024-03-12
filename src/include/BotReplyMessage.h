#pragma once

#include <Types.h>
#include <tgbot/Bot.h>
#include <tgbot/types/Message.h>

#include <string>

using TgBot::Bot;
using TgBot::Message;
using TgBot::MessageEntity;

/**
 * bot_sendReplyMessage - Send a reply message given a message
 *
 * @param bot Bot object
 * @param message message object to reply to
 * @param text text to reply with
 * @param replyToMsg optionally another message id to reply to, defaults to
 * message parameter's id
 * @param noError Do not throw exceptions when sending a reply message fails,
 * default false
 * @return The replied message object, if sent.
 */
static Message::Ptr _bot_sendReplyMessage(const Bot &bot,
                                          const Message::Ptr &message,
                                          const std::string &text,
                                          const MessageId replyToMsg = 0,
                                          const bool noError = false,
                                          const std::string parsemode = "") {
    return bot.getApi().sendMessage(
        message->chat->id, text, true,
        (replyToMsg == 0) ? message->messageId : replyToMsg, nullptr, parsemode,
        false, std::vector<MessageEntity::Ptr>(), noError);
}

static inline Message::Ptr bot_sendReplyMessage(
    const Bot &bot, const Message::Ptr &message, const std::string &text,
    const MessageId replyToMsg = 0, const bool noError = false,
    const std::string parsemode = "") {
    return _bot_sendReplyMessage(bot, message, text, replyToMsg, noError, "");
}

static inline Message::Ptr bot_sendReplyMessageMarkDown(
    const Bot &bot, const Message::Ptr &message, const std::string &text,
    const MessageId replyToMsg = 0, const bool noError = false) {
    return _bot_sendReplyMessage(bot, message, text, replyToMsg, noError,
                                 "markdown");
}

static inline Message::Ptr bot_sendReplyMessageHTML(
    const Bot &bot, const Message::Ptr &message, const std::string &text,
    const MessageId replyToMsg = 0, const bool noError = false) {
    return _bot_sendReplyMessage(bot, message, text, replyToMsg, noError,
                                 "html");
}

/**
 * bot_editMessage - Send a edit message request given a message
 *
 * @param bot Bot object
 * @param message message object to reply to
 * @param text text to edit
 * @return The replied message object, if sent.
 */
static inline Message::Ptr bot_editMessage(const Bot &bot,
                                           const Message::Ptr &message,
                                           const std::string &text) {
    return bot.getApi().editMessageText(text, message->chat->id,
                                        message->messageId);
}

static inline Message::Ptr bot_sendMessage(const Bot &bot, const ChatId chatid,
                                           const std::string &text) {
    return bot.getApi().sendMessage(chatid, text);
}
