#include <api/CommandModule.hpp>
#include <api/TgBotApi.hpp>
#include <database/bot/TgBotDatabaseImpl.hpp>

DECLARE_COMMAND_HANDLER(setowner) {
    auto* impl = provider->database.get();
    switch (impl->claimOwnerUserId(message->get<MessageAttrs::User>()->id)) {
        case DatabaseBase::OwnerClaimResult::OK:
            api->sendReplyMessage(message->message(),
                                  res->get(Strings::BOT_OWNER_SET));
            break;
        case DatabaseBase::OwnerClaimResult::ALREADY_SET:
            LOG(WARNING) << "#setowner rejected: owner already set";
            break;
        case DatabaseBase::OwnerClaimResult::BACKEND_ERROR:
            api->sendReplyMessage(message->message(),
                                  res->get(Strings::BACKEND_ERROR));
            break;
    }
}

extern "C" DYN_COMMAND_EXPORT const struct DynModule DYN_COMMAND_SYM = {
    .flags = DynModule::Flags::HideDescription,
    .name = "setowner",
    .description = "Set owner of the bot, for once",
    .function = COMMAND_HANDLER_NAME(setowner),
};
