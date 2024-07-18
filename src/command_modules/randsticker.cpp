#include <RandomNumberGenerator.hpp>
#include <TgBotWrapper.hpp>

using TgBot::StickerSet;

DECLARE_COMMAND_HANDLER(randsticker, wrapper, message) {
    MessageWrapper msg(wrapper, message);
    if (!msg.switchToReplyToMessage(
            "Sticker not found in replied-to message")) {
        return;
    }

    if (msg.hasSticker()) {
        auto sticker = msg.getSticker();
        random_return_type pos{};
        StickerSet::Ptr stickset;
        std::stringstream ss;
        try {
            stickset = wrapper->getStickerSet(sticker->setName);
        } catch (const std::exception& e) {
            wrapper->sendReplyMessage(message, e.what());
            return;
        }
        pos = RandomNumberGenerator::generate(stickset->stickers.size() - 1);
        wrapper->sendSticker(message, MediaIds(stickset->stickers[pos]));

        ss << "Sticker idx: " << pos + 1
           << " emoji: " << stickset->stickers[pos]->emoji << std::endl
           << "From pack " << std::quoted(stickset->title);
        wrapper->sendMessage(message, ss.str());
    }
}

DYN_COMMAND_FN(n, module) {
    module.command = "randsticker";
    module.description = "Random sticker from that pack";
    module.flags = CommandModule::Flags::None;
    module.fn = COMMAND_HANDLER_NAME(randsticker);
    return true;
}