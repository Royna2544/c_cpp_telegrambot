#include <fmt/core.h>

#include <Random.hpp>
#include <TgBotWrapper.hpp>

using TgBot::StickerSet;

DECLARE_COMMAND_HANDLER(randsticker, wrapper, message) {
    if (!message->replyToMessage_has<MessageExt::Attrs::Sticker>()) {
        wrapper->sendReplyMessage(message, "Reply to a sticker");
        return;
    }

    auto sticker = message->replyToMessage->sticker;
    Random::ret_type pos{};
    StickerSet::Ptr stickset;
    try {
        stickset = wrapper->getStickerSet(sticker->setName);
    } catch (const TgBot::TgException& e) {
        wrapper->sendReplyMessage(message, e.what());
        return;
    }
    pos = Random::getInstance()->generate(stickset->stickers.size() - 1);
    wrapper->sendSticker(message, MediaIds(stickset->stickers[pos]));

    const auto arg =
        fmt::format("Sticker idx: {} emoji: {}\nFrom pack: \"{}\"", pos + 1,
                    stickset->stickers[pos]->emoji, stickset->title);
    wrapper->sendMessage(message, arg);
}

DYN_COMMAND_FN(n, module) {
    module.command = "randsticker";
    module.description = "Random sticker from that pack";
    module.flags = CommandModule::Flags::None;
    module.function = COMMAND_HANDLER_NAME(randsticker);
    return true;
}