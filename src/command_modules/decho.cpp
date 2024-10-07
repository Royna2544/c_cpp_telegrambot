#include <absl/log/log.h>

#include <StringResManager.hpp>
#include <StringToolsExt.hpp>
#include <TgBotWrapper.hpp>

static DECLARE_COMMAND_HANDLER(decho, tgBotWrapper, message) {
    try {
        tgBotWrapper->deleteMessage(message);
    } catch (const TgBot::TgException &) {
        LOG(ERROR) << "Failed to delete message";
        // Cannot use delete echo in thie case.
        return;
    }
    if (message->has<MessageExt::Attrs::IsReplyMessage,
                     MessageExt::Attrs::ExtraText>()) {
        tgBotWrapper->copyAndReplyAsMessage(message, message->replyToMessage);
    }
    if (message->has<MessageExt::Attrs::IsReplyMessage>()) {
        tgBotWrapper->copyAndReplyAsMessage(message->replyToMessage);
    } else if (message->has<MessageExt::Attrs::ExtraText>()) {
        tgBotWrapper->sendMessage(message,
                                  message->get<MessageExt::Attrs::ExtraText>());
    }
}

DYN_COMMAND_FN(/*name*/, module) {
    module.command = "decho";
    module.description = GETSTR(DECHO_CMD_DESC);
    module.flags = CommandModule::Flags::None;
    module.function = COMMAND_HANDLER_NAME(decho);
    return true;
}
