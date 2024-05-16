#include <ExtArgs.h>

#include <algorithm>

#include "BotAddCommand.h"
#include "BotReplyMessage.h"
#include "CommandModule.h"
#include "StringToolsExt.h"

void CmdCommandFn(Bot& bot, const TgBot::Message::Ptr& message) {
    if (hasExtArgs(message)) {
        std::vector<std::string> args;
        splitAndClean(parseExtArgs(message), args, ' ');
        if (args.size() != 2) {
            bot_sendReplyMessage(bot, message,
                                 "Usage: /cmd <command> <reload|unload>");
            return;
        }
        const auto& command = args[0];
        const auto& action = args[1];
        if (action == "reload") {
            auto it = std::ranges::find_if(CommandModuleManager::loadedModules,
                                           [&command](const CommandModule& e) {
                                               return e.command == command;
                                           });
            if (it == CommandModuleManager::loadedModules.end()) {
                bot_sendReplyMessage(bot, message,
                                     "Command not found to reload");
                return;
            }
            if (it->isLoaded) {
                bot_sendReplyMessage(bot, message, "Command already loaded");
                return;
            }
            bot_AddCommand(bot, it->command, it->fn, it->isEnforced());
            bot_sendReplyMessage(bot, message, "Command reloaded");
        } else if (action == "unload") {
            auto it = std::ranges::find_if(CommandModuleManager::loadedModules,
                                           [&command](const CommandModule& e) {
                                               return e.command == command;
                                           });
            if (it == CommandModuleManager::loadedModules.end()) {
                bot_sendReplyMessage(bot, message,
                                     "Command not found to unload");
                return;
            }
            if (!it->isLoaded) {
                bot_sendReplyMessage(bot, message, "Command not loaded");
                return;
            }
            bot_RemoveCommand(bot, it->command);
            it->isLoaded = false;
            bot_sendReplyMessage(bot, message, "Command unloaded");
        } else {
            bot_sendReplyMessage(bot, message, "Unknown action " + action);
        }
    }
}

void loadcmd_cmd(CommandModule& module) {
    module.command = "cmd";
    module.description = "unload/reload a command";
    module.flags = CommandModule::Flags::Enforced;
    module.fn = CmdCommandFn;
}
