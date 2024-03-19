#include "BotReplyMessage.h"
#include "CommandModule.h"

static void UpdateCmdCommandFn(const Bot &bot, const Message::Ptr message) {
    CommandModule::updateBotCommands(bot);
    bot_sendReplyMessage(bot, message, "Updated successfully");
}

struct CommandModule cmd_updatecommands(
    "updatecommands", "Update bot's commands based on loaded modules",
    CommandModule::Flags::Enforced | CommandModule::Flags::HideDescription,
    UpdateCmdCommandFn);