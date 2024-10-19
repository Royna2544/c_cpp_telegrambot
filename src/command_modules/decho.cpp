#include <absl/log/log.h>

#include <StringResManager.hpp>
#include <StringToolsExt.hpp>
#include <api/CommandModule.hpp>
#include <api/MessageExt.hpp>
#include <api/TgBotApi.hpp>

#include <memory>
#include <string_view>
#include <tgbot/TgException.h>

static DECLARE_COMMAND_HANDLER(decho, tgBotWrapper, message) {
    try {
        tgBotWrapper->deleteMessage(message->message());
    } catch (const TgBot::TgException &) {
        LOG(ERROR) << "Failed to delete message";
        // Cannot use delete echo in thie case.
        return;
    }
    if (message->has({MessageAttrs::ExtraText}) && message->replyMessage()->exists()) {
        tgBotWrapper->copyAndReplyAsMessage(message->message(), message->replyMessage()->message());
    } else if (message->replyMessage()->exists()) {
        tgBotWrapper->copyAndReplyAsMessage(message->replyMessage()->message());
    } else if (message->has<MessageAttrs::ExtraText>()) {
        tgBotWrapper->sendMessage(message->get<MessageAttrs::Chat>(),
                                  message->get<MessageAttrs::ExtraText>());
    }
}

DYN_COMMAND_FN(/*name*/, module) {
    module.name = "decho";
    module.description = GETSTR(DECHO_CMD_DESC);
    module.flags = CommandModule::Flags::None;
    module.function = COMMAND_HANDLER_NAME(decho);
    return true;
}
