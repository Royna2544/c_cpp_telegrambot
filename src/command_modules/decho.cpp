#include <absl/log/log.h>

#include <TgBotWrapper.hpp>

static DECLARE_COMMAND_HANDLER(decho, tgBotWrapper, message) {
    const auto replyMsg = message->replyToMessage;
    MessageWrapper wrapper(message);

    try {
        tgBotWrapper->deleteMessage(message);
    } catch (const TgBot::TgException &) {
        // bot is not admin. nothing it can do
        LOG(WARNING) << "bot is not admin in chat '" << message->chat->title
                     << "', cannot use decho!";
        return;
    }
    if (wrapper.hasExtraText()) {
        std::string msg = wrapper.getExtraText();
        if (wrapper.hasReplyToMessage()) {
            wrapper.switchToReplyToMessage();
            tgBotWrapper->sendReplyMessage(wrapper.getMessage(), msg);
        } else {
            tgBotWrapper->sendMessage(message, msg);
        }
    } else if (replyMsg) {
        if (replyMsg->sticker) {
            tgBotWrapper->sendSticker(message, MediaIds(replyMsg->sticker));
        } else if (replyMsg->animation) {
            tgBotWrapper->sendAnimation(message, MediaIds(replyMsg->animation));
        } else if (!replyMsg->text.empty()) {
            tgBotWrapper->sendMessage(message, replyMsg->text);
        }
    }
}

DYN_COMMAND_FN(/*name*/, module) {
    module.command = "decho";
    module.description = "Delete and echo message";
    module.flags = CommandModule::Flags::None;
    module.fn = COMMAND_HANDLER_NAME(decho);
    return true;
}
