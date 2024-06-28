#include <database/bot/TgBotDatabaseImpl.hpp>
#include "BotReplyMessage.h"
#include "CommandModule.h"

static void setOwnerCommand(const Bot &bot, const Message::Ptr message) {
    auto impl = TgBotDatabaseImpl::getInstance();
    if (impl->getOwnerUserId() == DatabaseBase::kInvalidUserId) {
        impl->setOwnerUserId(message->from->id);
        bot_sendReplyMessage(bot, message, "Owner set");
    } else {
        LOG(WARNING) << "Your word rejected";
    }
}

void loadcmd_setowner(CommandModule& module) {
    module.command = "setowner";
    module.description = "Set owner of the bot, for once";
    module.flags = CommandModule::Flags::HideDescription;
    module.fn = setOwnerCommand;
}
