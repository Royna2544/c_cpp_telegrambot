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
    if (message->has({MessageAttrs::ExtraText}) && message->reply()->exists()) {
        api->copyAndReplyAsMessage(message->message(),
                                   message->reply()->message());
    } else if (message->reply()->exists()) {
        api->copyAndReplyAsMessage(message->reply()->message());
    } else if (message->has<MessageAttrs::ExtraText>()) {
        api->sendMessage(message->get<MessageAttrs::Chat>(),
                         message->get<MessageAttrs::ExtraText>());
    }
}

extern "C" const struct DynModule DYN_COMMAND_EXPORT DYN_COMMAND_SYM = {
    .flags = DynModule::Flags::None,
    .name = "decho",
    .description = "Delete and echo message",
    .function = COMMAND_HANDLER_NAME(decho),
    .valid_args =
        {
            .enabled = true,
            .counts = DynModule::craftArgCountMask<0, 1>(),
            .split_type = DynModule::ValidArgs::Split::None,
            .usage = "/decho [something-to-echo]",
        },
};
