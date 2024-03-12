#include <cmds.gen.h>

#include "BotReplyMessage.h"
#include "CommandModule.h"
#include "tgbot/types/BotCommand.h"

using TgBot::BotCommand;

static void UpdateCmdCommandFn(const Bot &bot, const Message::Ptr message) {
    std::vector<BotCommand::Ptr> buffer;
    for (const auto &cmd : gCmdModules) {
        if (!cmd->isHideDescription()) {
            auto onecommand = std::make_shared<CommandModule>(cmd);
            if (cmd->isEnforced())
                onecommand->description += " (Owner only)";
            buffer.emplace_back(onecommand);
        }
    }
    bot.getApi().setMyCommands(buffer);
    bot_sendReplyMessage(bot, message, "Updated successfully");
}

struct CommandModule cmd_updatecommands(
    "updatecommands", "Update bot's commands based on loaded modules",
    CommandModule::Flags::Enforced | CommandModule::Flags::HideDescription,
    UpdateCmdCommandFn);