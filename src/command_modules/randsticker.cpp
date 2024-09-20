#include <Random.hpp>
#include <TgBotWrapper.hpp>
#include "tgbot/TgException.h"

using TgBot::StickerSet;

DECLARE_COMMAND_HANDLER(randsticker, wrapper, message) {
    if (!message->replyToMessage_has<MessageExt::Attrs::Sticker>()) {
        wrapper->sendReplyMessage(message, "Reply to a sticker");
        return;
    }

    auto sticker = message->replyToMessage->sticker;
    Random::ret_type pos{};
    StickerSet::Ptr stickset;
    std::stringstream ss;
    try {
        stickset = wrapper->getStickerSet(sticker->setName);
    } catch (const TgBot::TgException& e) {
        wrapper->sendReplyMessage(message, e.what());
        return;
    }
    pos = Random::getInstance()->generate(stickset->stickers.size() - 1);
    wrapper->sendSticker(message, MediaIds(stickset->stickers[pos]));

    ss << "Sticker idx: " << pos + 1
       << " emoji: " << stickset->stickers[pos]->emoji << std::endl
       << "From pack " << std::quoted(stickset->title);
    wrapper->sendMessage(message, ss.str());
}

DYN_COMMAND_FN(n, module) {
    module.command = "randsticker";
    module.description = "Random sticker from that pack";
    module.flags = CommandModule::Flags::None;
    module.function = COMMAND_HANDLER_NAME(randsticker);
    return true;
}