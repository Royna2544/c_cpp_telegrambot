
#include <functional>

#include "BotAddCommand.h"
#include "CommandModule.h"
#include "StringToolsExt.h"

std::string CommandModuleManager::getLoadedModulesString() {
    std::stringstream ss;
    std::string outbuf;

    for (auto module : loadedModules) {
        ss << module.command << " ";
    }
    outbuf = ss.str();
    TrimStr(outbuf);
    return outbuf;
}

void CommandModuleManager::updateBotCommands(const Bot &bot) {
    std::vector<TgBot::BotCommand::Ptr> buffer;
    for (const auto &cmd : loadedModules) {
        if (!cmd.isHideDescription()) {
            auto onecommand = std::make_shared<CommandModule>(cmd);
            if (cmd.isEnforced()) {
                onecommand->description += " (Owner)";
            }
            buffer.emplace_back(onecommand);
        }
    }
    bot.getApi().setMyCommands(buffer);
}
