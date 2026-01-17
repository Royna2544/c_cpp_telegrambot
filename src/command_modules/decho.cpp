#include <absl/log/log.h>

#include <api/CommandModule.hpp>
#include <api/TgBotApi.hpp>
#include <api/types/ApiException.hpp>
#include <api/types/ParsedMessage.hpp>

constexpr std::string_view kDechoDeletePossibleMapKey = "decho_delete_possible";

DECLARE_COMMAND_HANDLER(decho) {
    if (message->has({MessageAttrs::ExtraText}) && message->reply()->exists()) {
        api->sendReplyMessage(message->reply()->message(),
                              message->get<MessageAttrs::ExtraText>());
    } else if (message->reply()->exists()) {
        api->copyAndReplyAsMessage(message->reply()->message());
    } else if (message->has<MessageAttrs::ExtraText>()) {
        api->sendMessage(message->get<MessageAttrs::Chat>(),
                         message->get<MessageAttrs::ExtraText>());
    }
    try {
        api->deleteMessage(message);
    } catch (const api::types::ApiException& e) {
        LOG(ERROR) << "Failed to delete message: " << e.what();
        // Cannot use delete echo in thie case.
        return;
#ifdef __ANDROID__
    } catch (...) {  // Only Termux acts like this
        LOG(ERROR) << "Failed to delete message";
        return;
#endif
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
