#include <fmt/format.h>
#include <tgbot/TgException.h>

#include <api/CommandModule.hpp>
#include <api/Providers.hpp>
#include <api/TgBotApi.hpp>

using TgBot::StickerSet;

DECLARE_COMMAND_HANDLER(randsticker) {
    if (!message->reply()->has<MessageAttrs::Sticker>()) {
        api->sendReplyMessage(message->message(),
                              res->get(Strings::REPLY_TO_A_STICKER));
        return;
    }

    auto sticker = message->reply()->get<MessageAttrs::Sticker>();
    if (!sticker->setName) {
        return;
    }
    Random::ret_type pos{};
    StickerSet::Ptr stickset;
    try {
        stickset = api->getStickerSet(*sticker->setName);
    } catch (const TgBot::TgException& e) {
        api->sendReplyMessage(message->message(), e.what());
        return;
    }
    pos = provider->random->generate(stickset->stickers.size() - 1);
    api->sendSticker(message->get<MessageAttrs::Chat>(),
                     MediaIds(stickset->stickers[pos]));

    const auto arg = fmt::format(
        "Sticker idx: {} emoji: {}\nFrom pack: \"{}\"", pos + 1,
        stickset->stickers[pos]->emoji.value_or("none"), stickset->title);
    api->sendMessage(message->get<MessageAttrs::Chat>(), arg);
}

extern "C" const struct DynModule DYN_COMMAND_EXPORT DYN_COMMAND_SYM = {
    .flags = DynModule::Flags::None,
    .name = "randsticker",
    .description = "Random sticker from that pack",
    .function = COMMAND_HANDLER_NAME(randsticker),
    .valid_args = {.enabled = true,
                   .counts = DynModule::craftArgCountMask<0>()},
};