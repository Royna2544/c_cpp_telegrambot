#include <api/CommandModule.hpp>
#include <api/TgBotApi.hpp>
#include <database/bot/TgBotDatabaseImpl.hpp>

DECLARE_COMMAND_HANDLER(setowner) {
    auto *impl = provider->database.get();
    if (!impl->getOwnerUserId().has_value()) {
        impl->setOwnerUserId(message->get<MessageAttrs::User>()->id);
        api->sendReplyMessage(message->message(),
                              access(res, Strings::BOT_OWNER_SET));
    } else {
        LOG(WARNING) << "Your word rejected";
    }
}

extern "C" const struct DynModule DYN_COMMAND_EXPORT DYN_COMMAND_SYM = {
    .flags = DynModule::Flags::HideDescription,
    .name = "setowner",
    .description = "Set owner of the bot, for once",
    .function = COMMAND_HANDLER_NAME(setowner),
};
