#include <fmt/format.h>

#include <StringResManager.hpp>
#include <api/CommandModule.hpp>
#include <api/TgBotApi.hpp>

DECLARE_COMMAND_HANDLER(cmd, botWrapper, message) {
    const auto& args = message->get<MessageAttrs::ParsedArgumentsList>();
    const auto& command = args[0];
    const auto& action = args[1];
    bool ret = false;
    bool valid = false;

    std::string result_message;
    if (action == "reload") {
        ret = botWrapper->reloadCommand(command);
        valid = true;
    } else if (action == "unload") {
        ret = botWrapper->unloadCommand(command);
        valid = true;
    }

    if (valid) {
        if (ret) {
            result_message = fmt::format("{} {}: {}", command, action,
                                         GETSTR(OPERATION_SUCCESSFUL));
        } else {
            result_message = fmt::format("{} {}: {}", command, action,
                                         GETSTR(OPERATION_FAILURE));
        }
    } else {
        result_message = fmt::format("{}: {}", GETSTR(UNKNOWN_ACTION), action);
    }
    botWrapper->sendReplyMessage(message->message(), result_message);
}

DYN_COMMAND_FN(/*name*/, module) {
    module.name = "cmd";
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
