#include <ExtArgs.h>
#include <absl/log/log.h>
#include <MessageWrapper.hpp>

#include "BotReplyMessage.h"
#include "CommandModule.h"

static void DeleteEchoCommandFn(const Bot &bot, const Message::Ptr message) {
    const auto replyMsg = message->replyToMessage;
    const auto chatId = message->chat->id;
    MessageWrapper wrapper(bot, message);

    try {
        bot.getApi().deleteMessage(chatId, message->messageId);
    } catch (const TgBot::TgException &) {
        // bot is not admin. nothing it can do
        LOG(WARNING) << "bot is not admin in chat '" << message->chat->title
                     << "', cannot use decho!";
        return;
    }
    if (wrapper.hasExtraText()) {
        std::string msg = wrapper.getExtraText();
        bot_sendReplyMessage(bot, message, msg,
                             (replyMsg) ? replyMsg->messageId : 0, true);
    } else if (replyMsg) {
        if (replyMsg->sticker) {
            bot_sendSticker(bot, message->chat, replyMsg->sticker);
        } else if (replyMsg->animation) {
            bot.getApi().sendAnimation(message->chat->id,
                                       replyMsg->animation->fileId);
        } else if (replyMsg->video) {
            bot.getApi().sendVideo(message->chat->id, replyMsg->video->fileId);
        } else if (!replyMsg->photo.empty()) {
            bot.getApi().sendPhoto(
                message->chat->id, replyMsg->photo.front()->fileId,
                "(Note: Sending all photos are not supported)");
        } else if (!replyMsg->text.empty()) {
            bot_sendReplyMessage(bot, message, replyMsg->text,
                                 replyMsg->messageId);
        }
    }
}

void loadcmd_decho(CommandModule& module) {
    module.command = "decho";
    module.description = "Delete and echo message";
    module.flags = CommandModule::Flags::None;
    module.fn = DeleteEchoCommandFn;
}
