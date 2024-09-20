#include <StringResManager.hpp>
#include <TgBotWrapper.hpp>

DECLARE_COMMAND_HANDLER(cmd, botWrapper, message) {
    const auto& args = message->arguments();
    const auto& command = args[0];
    const auto& action = args[1];
    bool ret = false;
    if (action == "reload") {
        ret = botWrapper->reloadCommand(command);
    } else if (action == "unload") {
        ret = botWrapper->unloadCommand(command);
    } else {
        botWrapper->sendReplyMessage(message,
                                     GETSTR_IS(UNKNOWN_ACTION) + action);
        return;
    }
    if (ret) {
        botWrapper->sendReplyMessage(message,
                                     GETSTR_IS(OPERATION_SUCCESSFUL) + command);
    } else {
        botWrapper->sendReplyMessage(message,
                                     GETSTR_IS(OPERATION_FAILURE) + command);
    }
}

DYN_COMMAND_FN(/*name*/, module) {
    module.command = "cmd";
    module.description = "unload/reload a command";
    module.flags = CommandModule::Flags::Enforced;
    module.function = COMMAND_HANDLER_NAME(cmd);
    module.valid_arguments.enabled = true;
    module.valid_arguments.counts.emplace_back(2);
    module.valid_arguments.split_type =
        CommandModule::ValidArgs::Split::ByWhitespace;
    module.valid_arguments.usage = "/cmd <cmdname> <reload/unload>";
    return true;
}
