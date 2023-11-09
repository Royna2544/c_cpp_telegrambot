#pragma once

#include <tgbot/Bot.h>
#include <tgbot/types/Message.h>
#include <Types.h>

#include <string>

using TgBot::Bot;
using TgBot::Message;
using TgBot::MessageEntity;

static inline Message::Ptr bot_sendReplyMessage(const Bot &bot, const Message::Ptr &message,
                                        const std::string &text, const MessageId replyToMsg = 0,
                                        const bool noError = false) {
    return bot.getApi().sendMessage(message->chat->id, text,
                             true, (replyToMsg == 0) ? message->messageId : replyToMsg,
                             nullptr, "", false, std::vector<MessageEntity::Ptr>(), noError);
}
