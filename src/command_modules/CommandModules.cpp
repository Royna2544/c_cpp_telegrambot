
#include <StringResManager.hpp>
#include <boost/algorithm/string/trim.hpp>

#include "CommandModule.h"

std::string CommandModuleManager::getLoadedModulesString() {
    std::stringstream ss;
    std::string outbuf;

    for (auto module : loadedModules) {
        ss << module.command << " ";
    }
    outbuf = ss.str();
    boost::trim(outbuf);
    return outbuf;
}

void CommandModuleManager::updateBotCommands(const Bot &bot) {
    std::vector<TgBot::BotCommand::Ptr> buffer;
    for (const auto &cmd : loadedModules) {
        if (!cmd.isHideDescription()) {
            auto onecommand = std::make_shared<CommandModule>(cmd);
            if (cmd.isEnforced()) {
                onecommand->description += " " + GETSTR_BRACE(OWNER);
            }
            buffer.emplace_back(onecommand);
        }
    }
    try {
        bot.getApi().setMyCommands(buffer);
    } catch (const TgBot::TgException &e) {
        LOG(ERROR) << GETSTR_IS(ERROR_UPDATING_BOT_COMMANDS) << e.what()
                   << std::endl;
    }
}
