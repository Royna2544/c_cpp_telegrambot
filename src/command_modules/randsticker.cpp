#include <fmt/format.h>
#include <tgbot/TgException.h>

#include <Random.hpp>
#include <api/CommandModule.hpp>
#include <api/TgBotApi.hpp>

using TgBot::StickerSet;

DECLARE_COMMAND_HANDLER(randsticker, wrapper, message) {
    if (!message->replyMessage()->has<MessageAttrs::Sticker>()) {
        wrapper->sendReplyMessage(message->message(), "Reply to a sticker");
        return;
    }

    auto sticker = message->replyMessage()->get<MessageAttrs::Sticker>();
    Random::ret_type pos{};
    StickerSet::Ptr stickset;
    try {
        stickset = wrapper->getStickerSet(sticker->setName);
    } catch (const TgBot::TgException& e) {
        wrapper->sendReplyMessage(message->message(), e.what());
        return;
    }
    pos = Random::getInstance()->generate(stickset->stickers.size() - 1);
    wrapper->sendSticker(message->get<MessageAttrs::Chat>(),
                         MediaIds(stickset->stickers[pos]));

    const auto arg =
        fmt::format("Sticker idx: {} emoji: {}\nFrom pack: \"{}\"", pos + 1,
                    stickset->stickers[pos]->emoji, stickset->title);
    wrapper->sendMessage(message->get<MessageAttrs::Chat>(), arg);
}

DYN_COMMAND_FN(n, module) {
    module.name = "randsticker";
    module.description = "Random sticker from that pack";
    module.flags = CommandModule::Flags::None;
    module.function = COMMAND_HANDLER_NAME(randsticker);
    return true;
}