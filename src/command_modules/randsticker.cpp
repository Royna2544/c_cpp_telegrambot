#include <fmt/format.h>
#include <tgbot/TgException.h>

#include <api/CommandModule.hpp>
#include <api/Providers.hpp>
#include <api/TgBotApi.hpp>

using TgBot::StickerSet;

DECLARE_COMMAND_HANDLER(randsticker) {
    if (!message->replyMessage()->has<MessageAttrs::Sticker>()) {
        api->sendReplyMessage(message->message(),
                                  access(res, Strings::REPLY_TO_A_STICKER));
        return;
    }

    auto sticker = message->replyMessage()->get<MessageAttrs::Sticker>();
    Random::ret_type pos{};
    StickerSet::Ptr stickset;
    try {
        stickset = api->getStickerSet(sticker->setName);
    } catch (const TgBot::TgException& e) {
        api->sendReplyMessage(message->message(), e.what());
        return;
    }
    pos = provider->random->generate(stickset->stickers.size() - 1);
    api->sendSticker(message->get<MessageAttrs::Chat>(),
                         MediaIds(stickset->stickers[pos]));

    const auto arg =
        fmt::format("Sticker idx: {} emoji: {}\nFrom pack: \"{}\"", pos + 1,
                    stickset->stickers[pos]->emoji, stickset->title);
    api->sendMessage(message->get<MessageAttrs::Chat>(), arg);
}

DYN_COMMAND_FN(n, module) {
    module.name = "randsticker";
    module.description = "Random sticker from that pack";
    module.flags = CommandModule::Flags::None;
    module.function = COMMAND_HANDLER_NAME(randsticker);
    return true;
}