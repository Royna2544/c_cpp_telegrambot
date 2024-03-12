#include <ExtArgs.h>
#include <random/RandomNumberGenerator.h>

#include "CommandModule.h"

using TgBot::StickerSet;

static void RandomStickerCommandFn(const Bot &bot, const Message::Ptr message) {
    if (message->replyToMessage && message->replyToMessage->sticker) {
        StickerSet::Ptr stickset;
        try {
            stickset = bot.getApi().getStickerSet(
                message->replyToMessage->sticker->setName);
        } catch (const std::exception &e) {
            bot_sendReplyMessage(bot, message, e.what());
            return;
        }
        const ssize_t pos = genRandomNumber(stickset->stickers.size() - 1);
        bot.getApi().sendSticker(message->chat->id,
                                 stickset->stickers[pos]->fileId,
                                 message->messageId, nullptr, false, true);
        std::stringstream ss;
        ss << "Sticker idx: " << pos + 1
           << " emoji: " << stickset->stickers[pos]->emoji << std::endl
           << "From pack \"" + stickset->title + "\"";
        bot_sendReplyMessage(bot, message, ss.str());
    } else {
        bot_sendReplyMessage(bot, message,
                             "Sticker not found in replied-to message");
    }
}

struct CommandModule cmd_randsticker("randsticker",
    "Get a random sticker from a replyed-to-msg-s-stickerpack",
    CommandModule::Flags::None, RandomStickerCommandFn);