#include <ExtArgs.h>
#include <random/RandomNumberGenerator.h>

#include "BotReplyMessage.h"
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
        const size_t pos = genRandomNumber(stickset->stickers.size() - 1);
        bot_sendSticker(bot, message, stickset->stickers[pos], message);
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
    "Random sticker from that pack",
    CommandModule::Flags::None, RandomStickerCommandFn);