#include <absl/log/log.h>
#include <api/types/ApiException.hpp>

#include <api/CommandModule.hpp>
#include <api/types/ParsedMessage.hpp>
#include <api/TgBotApi.hpp>

DECLARE_COMMAND_HANDLER(decho) {
    try {
        api->deleteMessage(message);
    } catch (const api::types::ApiException &e) {
        LOG(ERROR) << "Failed to delete message: " << e.what();
        // Cannot use delete echo in thie case.
        return;
#ifdef __ANDROID__
    } catch (...) {  // Only Termux acts like this
        LOG(ERROR) << "Failed to delete message";
        return;
#endif
    }
    if (message.has({api::types::ParsedMessage::Attrs::ExtraText}) && message.replyToMessage.has_value()) {
        api->copyAndReplyAsMessage(message,
                                   message.replyToMessage.value());
    } else if (message.replyToMessage.has_value()) {
        api->copyAndReplyAsMessage(message.replyToMessage.value());
    } else if (message.has<api::types::ParsedMessage::Attrs::ExtraText>()) {
        api->sendMessage(message.get<api::types::ParsedMessage::Attrs::Chat>(),
            message.get<api::types::ParsedMessage::Attrs::ExtraText>());
    }
}

extern "C" DYN_COMMAND_EXPORT const struct DynModule DYN_COMMAND_SYM = {
    .flags = DynModule::Flags::None,
    .name = "decho",
    .description = "Delete and echo message",
    .function = COMMAND_HANDLER_NAME(decho),
    .valid_args =
        {
            .enabled = true,
            .counts = DynModule::craftArgCountMask<0, 1>(),
            .split_type = DynModule::ValidArgs::SplitMethod::None,
            .usage = "/decho [something-to-echo]",
        },
};
