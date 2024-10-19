#include <database/bot/TgBotDatabaseImpl.hpp>
#include <api/CommandModule.hpp>
#include <api/TgBotApi.hpp>

DECLARE_COMMAND_HANDLER(setowner, tgWrapper, message) {
    auto *impl = TgBotDatabaseImpl::getInstance();
    if (!impl->getOwnerUserId().has_value()) {
        impl->setOwnerUserId(message->get<MessageAttrs::User>()->id);
        tgWrapper->sendReplyMessage(message->message(), "Owner set");
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
