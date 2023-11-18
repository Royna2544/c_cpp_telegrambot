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
 * @param replyToMsg optionally another message id to reply to, defaults to message parameter's id
 * @param noError Do not throw exceptions when sending a reply message fails, default false
 * @return The replied message object, if sent.
 */
static inline Message::Ptr bot_sendReplyMessage(const Bot &bot, const Message::Ptr &message,
                                                const std::string &text, const MessageId replyToMsg = 0,
                                                const bool noError = false) {
    return bot.getApi().sendMessage(message->chat->id, text,
                                    true, (replyToMsg == 0) ? message->messageId : replyToMsg,
                                    nullptr, "", false, std::vector<MessageEntity::Ptr>(), noError);
}

/**
 * bot_editMessage - Send a edit message request given a message
 *
 * @param bot Bot object
 * @param message message object to reply to
 * @param text text to edit
 * @return The replied message object, if sent.
 */
static inline Message::Ptr bot_editMessage(const Bot &bot, const Message::Ptr &message,
                                           const std::string &text) {
    return bot.getApi().editMessageText(text, message->chat->id, message->messageId);
}
