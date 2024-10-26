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

DYN_COMMAND_FN(n, module) {
    module.name = "setowner";
    module.description = "Set owner of the bot, for once";
    module.flags = CommandModule::Flags::HideDescription;
    module.function = COMMAND_HANDLER_NAME(setowner);
    return true;
}
