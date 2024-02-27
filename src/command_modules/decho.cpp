#include <ExtArgs.h>
#include <Logging.h>

#include "CommandModule.h"

static void DeleteEchoCommandFn(const Bot &bot, const Message::Ptr message) {
    const auto replyMsg = message->replyToMessage;
    const auto chatId = message->chat->id;

    try {
        bot.getApi().deleteMessage(chatId, message->messageId);
    } catch (const TgBot::TgException &) {
        // bot is not admin. nothing it can do
        LOG_W("bot is not admin in chat '%s', cannot use decho!",
              message->chat->title.c_str());
        return;
    }
    if (hasExtArgs(message)) {
        std::string msg;
        parseExtArgs(message, msg);
        bot_sendReplyMessage(bot, message, msg,
                             (replyMsg) ? replyMsg->messageId : 0, true);
    } else if (replyMsg) {
        if (replyMsg->sticker) {
            bot.getApi().sendSticker(message->chat->id, replyMsg->sticker->fileId);
        } else if (replyMsg->animation) {
            bot.getApi().sendAnimation(message->chat->id, replyMsg->animation->fileId);
        } else if (replyMsg->video) {
            bot.getApi().sendVideo(message->chat->id, replyMsg->video->fileId);
        } else if (!replyMsg->photo.empty()) {
            bot.getApi().sendPhoto(message->chat->id, replyMsg->photo.front()->fileId,
                                   "(Note: Sending all photos are not supported)");
        } else if (!replyMsg->text.empty()) {
            bot_sendReplyMessage(bot, message, replyMsg->text, replyMsg->messageId);
        }
    }
}

struct CommandModule cmd_decho {
    .enforced = false,
    .name = "decho",
    .fn = DeleteEchoCommandFn,
};