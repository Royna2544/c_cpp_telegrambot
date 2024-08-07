#include <StringResManager.hpp>
#include <TgBotWrapper.hpp>
#include <boost/algorithm/string/split.hpp>

#include "StringToolsExt.hpp"

DECLARE_COMMAND_HANDLER(cmd, botWrapper, message) {
    MessageWrapper wrapper(botWrapper, message);
    if (wrapper.hasExtraText()) {
        std::vector<std::string> args;

        boost::split(args, wrapper.getExtraText(), isWhitespace);
        if (args.size() != 2) {
            wrapper.sendMessageOnExit("Usage: /cmd <command> <reload|unload>");
            return;
        }
        const auto& command = args[0];
        const auto& action = args[1];
        bool ret = false;
        if (action == "reload") {
            ret = botWrapper->reloadCommand(command);
        } else if (action == "unload") {
            ret = botWrapper->unloadCommand(command);
        } else {
            wrapper.sendMessageOnExit(GETSTR_IS(UNKNOWN_ACTION) + action);
            return;
        }
        if (ret) {
            wrapper.sendMessageOnExit(GETSTR_IS(OPERATION_SUCCESSFUL) +
                                      command);
        } else {
            wrapper.sendMessageOnExit(GETSTR_IS(OPERATION_FAILURE) + command);
        }
    }
}

DYN_COMMAND_FN(/*name*/, module) {
    module.command = "cmd";
    module.description = "unload/reload a command";
    module.flags = CommandModule::Flags::Enforced;
    module.fn = COMMAND_HANDLER_NAME(cmd);
    return true;
}
