#pragma once

#include <Types.h>
#include <tgbot/Bot.h>
#include <tgbot/types/Message.h>

#include <string>

using TgBot::Animation;
using TgBot::Bot;
using TgBot::Chat;
using TgBot::Message;
using TgBot::MessageEntity;
using TgBot::Sticker;

/**
 * @brief sends a reply message to a given message
 *
 * @param bot the bot object
 * @param message the message object to reply to
 * @param text the text of the reply message
 * @param replyToMsg the message id to reply to, 0 for none
 * @param noError whether to suppress errors
 * @return the replied message object, if sent
 */
extern Message::Ptr bot_sendReplyMessage(const Bot &bot,
                                         const Message::Ptr &message,
                                         const std::string &text,
                                         const MessageId replyToMsg = 0,
                                         const bool noError = false);

/**
 * @brief sends a reply message to a given message
 * With text interpreted as markdown
 *
 * @param bot the bot object
 * @param message the message object to reply to
 * @param text the text of the reply message
 * @param replyToMsg the message id to reply to, 0 for none
 * @param noError whether to suppress errors
 * @return the replied message object, if sent
 */
extern Message::Ptr bot_sendReplyMessageMarkDown(const Bot &bot,
                                                 const Message::Ptr &message,
                                                 const std::string &text,
                                                 const MessageId replyToMsg = 0,
                                                 const bool noError = false);

/**
 * @brief sends a reply message to a given message
 * With text interpreted as HTML
 *
 * @param bot the bot object
 * @param message the message object to reply to
 * @param text the text of the reply message
 * @param replyToMsg the message id to reply to, 0 for none
 * @param noError whether to suppress errors
 * @return the replied message object, if sent
 */
extern Message::Ptr bot_sendReplyMessageHTML(const Bot &bot,
                                             const Message::Ptr &message,
                                             const std::string &text,
                                             const MessageId replyToMsg = 0,
                                             const bool noError = false);

/**
 * bot_editMessage - Send a edit message request given a message
 *
 * @param bot Bot object
 * @param message message object to reply to
 * @param text text to edit
 * @return The replied message object, if sent.
 */
extern Message::Ptr bot_editMessage(const Bot &bot, const Message::Ptr &message,
                                    const std::string &text);

extern Message::Ptr bot_sendMessage(const Bot &bot, const ChatId chatid,
                                    const std::string &text);

extern Message::Ptr bot_sendSticker(const Bot &bot, const ChatId &chatid,
                                    Sticker::Ptr sticker,
                                    const Message::Ptr &replyTo);

extern Message::Ptr bot_sendSticker(const Bot &bot, const ChatId &chat,
                                    Sticker::Ptr sticker);

extern Message::Ptr bot_sendAnimation(const Bot &bot, const ChatId &chat,
                                      Animation::Ptr gif,
                                      const Message::Ptr &replyTo);

extern Message::Ptr bot_sendAnimation(const Bot &bot, const ChatId &chat,
                                      Animation::Ptr gif);

static inline Message::Ptr bot_sendAnimation(const Bot &bot,
                                             const Chat::Ptr &chat,
                                             Animation::Ptr gif) {
    return bot_sendAnimation(bot, chat->id, gif);
}

static inline Message::Ptr bot_sendSticker(const Bot &bot,
                                           const Chat::Ptr &chat,
                                           Sticker::Ptr stick) {
    return bot_sendSticker(bot, chat->id, stick);
}


static inline Message::Ptr bot_sendAnimation(const Bot &bot,
                                             const Chat::Ptr &chat,
                                             Animation::Ptr gif, const Message::Ptr& replyTo) {
    return bot_sendAnimation(bot, chat->id, gif, replyTo);
}

static inline Message::Ptr bot_sendSticker(const Bot &bot,
                                           const Chat::Ptr &chat,
                                           Sticker::Ptr stick,  const Message::Ptr& replyTo) {
    return bot_sendSticker(bot, chat->id, stick, replyTo);
}