#include <BotReplyMessage.h>

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

Message::Ptr bot_sendReplyMessage(const Bot &bot, const Message::Ptr &message,
                                  const std::string &text,
                                  const MessageId replyToMsg,
                                  const bool noError) {
    return _bot_sendReplyMessage(bot, message, text, replyToMsg, noError, "");
}

Message::Ptr bot_sendReplyMessageMarkDown(const Bot &bot,
                                          const Message::Ptr &message,
                                          const std::string &text,
                                          const MessageId replyToMsg,
                                          const bool noError) {
    return _bot_sendReplyMessage(bot, message, text, replyToMsg, noError,
                                 "markdown");
}

Message::Ptr bot_sendReplyMessageHTML(const Bot &bot,
                                      const Message::Ptr &message,
                                      const std::string &text,
                                      const MessageId replyToMsg,
                                      const bool noError) {
    return _bot_sendReplyMessage(bot, message, text, replyToMsg, noError,
                                 "html");
}

Message::Ptr bot_editMessage(const Bot &bot, const Message::Ptr &message,
                             const std::string &text) {
    return bot.getApi().editMessageText(text, message->chat->id,
                                        message->messageId);
}

Message::Ptr bot_sendMessage(const Bot &bot, const ChatId chatid,
                             const std::string &text) {
    return bot.getApi().sendMessage(chatid, text);
}

Message::Ptr bot_sendSticker(const Bot &bot, const ChatId &chatid,
                             Sticker::Ptr sticker,
                             const Message::Ptr &replyTo) {
    return bot.getApi().sendSticker(chatid, sticker->fileId,
                                    replyTo->messageId);
}

Message::Ptr bot_sendSticker(const Bot &bot, const ChatId &chat,
                             Sticker::Ptr sticker) {
    return bot.getApi().sendSticker(chat, sticker->fileId);
}

Message::Ptr bot_sendAnimation(const Bot &bot, const ChatId &chat,
                               Animation::Ptr gif,
                               const Message::Ptr &replyTo) {
    return bot.getApi().sendSticker(chat, gif->fileId, replyTo->messageId);
}

Message::Ptr bot_sendAnimation(const Bot &bot, const ChatId &chat,
                               Animation::Ptr gif) {
    return bot.getApi().sendSticker(chat, gif->fileId);
}
