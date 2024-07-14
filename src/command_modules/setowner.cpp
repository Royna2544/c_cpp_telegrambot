#include <database/bot/TgBotDatabaseImpl.hpp>
#include <TgBotWrapper.hpp>

static void setOwnerCommand(const TgBotWrapper *tgWrapper, MessagePtr message) {
    auto impl = TgBotDatabaseImpl::getInstance();
    if (!impl->getOwnerUserId().has_value()) {
        impl->setOwnerUserId(message->from->id);
        tgWrapper->sendReplyMessage(message, "Owner set");
    } else {
        LOG(WARNING) << "Your word rejected";
    }
}

DYN_COMMAND_FN(n, module) {
    module.command = "setowner";
    module.description = "Set owner of the bot, for once";
    module.flags = CommandModule::Flags::HideDescription;
    module.fn = setOwnerCommand;
    return true;
}
