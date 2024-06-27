#include <BotReplyMessage.h>
#include "CommandModule.h"
#include <random/RandomNumberGenerator.h>

#include <MessageWrapper.hpp>

using TgBot::StickerSet;

static void RandomStickerCommandFn(const Bot &bot, const Message::Ptr& message) {
    MessageWrapper msg(bot, message);
    if (!msg.switchToReplyToMessage("Sticker not found in replied-to message")) {
        return;
    }

    if (msg.hasSticker()) {
        auto sticker = msg.getSticker();
        random_return_type pos{};
        StickerSet::Ptr stickset;
        std::stringstream ss;
        try {
            stickset = bot.getApi().getStickerSet(sticker->setName);
        } catch (const std::exception &e) {
            bot_sendReplyMessage(bot, message, e.what());
            return;
        }
        pos = RandomNumberGenerator::generate(stickset->stickers.size() - 1);
        bot_sendSticker(bot, message->chat, stickset->stickers[pos], message);

        ss << "Sticker idx: " << pos + 1
           << " emoji: " << stickset->stickers[pos]->emoji << std::endl
           << "From pack " << std::quoted(stickset->title);
        bot_sendReplyMessage(bot, message, ss.str());
    }
}

void loadcmd_randsticker(CommandModule &module) {
    module.command = "randsticker";
    module.description = "Random sticker from that pack";
    module.flags = CommandModule::Flags::None;
    module.fn = RandomStickerCommandFn;
}