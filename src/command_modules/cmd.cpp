#include <ExtArgs.h>

#include <MessageWrapper.hpp>
#include <algorithm>
#include <boost/algorithm/string/split.hpp>

#include "BotAddCommand.h"
#include "BotReplyMessage.h"
#include "CommandModule.h"
#include "StringToolsExt.hpp"

namespace {
void handle_reload(Bot& bot, MessageWrapper& wrapper, std::string command) {
    auto it = std::ranges::find_if(
        CommandModuleManager::loadedModules,
        [&command](const CommandModule& e) { return e.command == command; });
    if (it == CommandModuleManager::loadedModules.end()) {
        wrapper.sendMessageOnExit("Command not found to reload");
        return;
    }
    if (it->isLoaded) {
        wrapper.sendMessageOnExit("Command already loaded");
        return;
    }
    bot_AddCommand(bot, it->command, it->fn, it->isEnforced());
    wrapper.sendMessageOnExit("Command reloaded");
}

void handle_unload(Bot& bot, MessageWrapper& wrapper, std::string command) {
    auto it = std::ranges::find_if(
        CommandModuleManager::loadedModules,
        [&command](const CommandModule& e) { return e.command == command; });
    if (it == CommandModuleManager::loadedModules.end()) {
        wrapper.sendMessageOnExit("Command not found to unload");
        return;
    }
    if (!it->isLoaded) {
        wrapper.sendMessageOnExit("Command not loaded");
        return;
    }
    bot_RemoveCommand(bot, it->command);
    it->isLoaded = false;
    wrapper.sendMessageOnExit("Command unloaded");
}
}  // namespace

void CmdCommandFn(Bot& bot, const TgBot::Message::Ptr& message) {
    MessageWrapper wrapper(bot, message);
    if (wrapper.hasExtraText()) {
        std::vector<std::string> args;

        boost::split(args, wrapper.getExtraText(), isWhitespace);
        if (args.size() != 2) {
            wrapper.sendMessageOnExit("Usage: /cmd <command> <reload|unload>");
            return;
        }
        const auto& command = args[0];
        const auto& action = args[1];
        if (action == "reload") {
            handle_reload(bot, wrapper, command);
        } else if (action == "unload") {
            handle_unload(bot, wrapper, command);
        } else {
            wrapper.sendMessageOnExit("Unknown action " + action);
        }
    }
}

void loadcmd_cmd(CommandModule& module) {
    module.command = "cmd";
    module.description = "unload/reload a command";
    module.flags = CommandModule::Flags::Enforced;
    module.fn = CmdCommandFn;
}
