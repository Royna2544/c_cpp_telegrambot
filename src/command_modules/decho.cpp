#include <absl/log/log.h>
#include <tgbot/TgException.h>

#include <api/CommandModule.hpp>
#include <api/MessageExt.hpp>
#include <api/TgBotApi.hpp>

DECLARE_COMMAND_HANDLER(decho) {
    try {
        api->deleteMessage(message->message());
    } catch (const TgBot::TgException &) {
        LOG(ERROR) << "Failed to delete message";
        // Cannot use delete echo in thie case.
        return;
    }
    if (message->has({MessageAttrs::ExtraText}) &&
        message->reply()->exists()) {
        api->copyAndReplyAsMessage(message->message(),
                                   message->reply()->message());
    } else if (message->reply()->exists()) {
        api->copyAndReplyAsMessage(message->reply()->message());
    } else if (message->has<MessageAttrs::ExtraText>()) {
        api->sendMessage(message->get<MessageAttrs::Chat>(),
                         message->get<MessageAttrs::ExtraText>());
    }
}

DYN_COMMAND_FN(/*name*/, module) {
    module.name = "decho";
    module.description = "Delete and echo message";
    module.flags = CommandModule::Flags::None;
    module.function = COMMAND_HANDLER_NAME(decho);
    return true;
}
