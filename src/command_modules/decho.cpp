#include <absl/log/log.h>
#include <tgbot/TgException.h>
#include <trivial_helpers/_tgbot.h>

#include <api/CommandModule.hpp>
#include <api/MessageExt.hpp>
#include <api/TgBotApi.hpp>
#include <memory>

#include "tgbot/types/ChatMemberAdministrator.h"

DECLARE_COMMAND_HANDLER(decho) {
    auto chmember = api->getChatMember(message->get<MessageAttrs::Chat>()->id,
                                       api->getBotUser()->id);
    if (chmember->status != TgBot::ChatMemberAdministrator::STATUS) {
        LOG(WARNING) << fmt::format(
            "Bot is not admin in the chat {}, cannot use decho.",
            message->get<MessageAttrs::Chat>());
        return;
    }

    if (!std::static_pointer_cast<TgBot::ChatMemberAdministrator>(chmember)
             ->canDeleteMessages) {
        LOG(WARNING) << fmt::format(
            "Bot does not have delete messages permission in chat {}, "
            "cannot use decho.",
            message->get<MessageAttrs::Chat>());
        return;
    }
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
        api->deleteMessage(message->message());
    } catch (const TgBot::TgException&) {
        // Why would this really happen?
        LOG(ERROR) << "Failed to delete message";
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
            .split_type = DynModule::ValidArgs::Split::None,
            .usage = "/decho [something-to-echo]",
        },
};
