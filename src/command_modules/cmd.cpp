#include <fmt/format.h>

#include <api/CommandModule.hpp>
#include <api/StringResLoader.hpp>
#include <api/TgBotApi.hpp>

DECLARE_COMMAND_HANDLER(cmd) {
    const auto& args = message->get<MessageAttrs::ParsedArgumentsList>();
    const auto& command = args[0];
    const auto& action = args[1];
    bool ret = false;
    bool valid = false;

    std::string result_message;
    if (action == "reload") {
        ret = api->reloadCommand(command);
        valid = true;
    } else if (action == "unload") {
        ret = api->unloadCommand(command);
        valid = true;
    }

    if (valid) {
        if (ret) {
            result_message =
                fmt::format("{} {}: {}", command, action,
                            res->get(Strings::OPERATION_SUCCESSFUL));
        } else {
            result_message = fmt::format("{} {}: {}", command, action,
                                         res->get(Strings::OPERATION_FAILURE));
        }
    } else {
        result_message =
            fmt::format("{}: {}", res->get(Strings::UNKNOWN_ACTION), action);
    }
    api->sendReplyMessage(message->message(), result_message);
}

extern "C" DYN_COMMAND_EXPORT const struct DynModule DYN_COMMAND_SYM = {
    .flags = DynModule::Flags::Enforced,
    .name = "cmd",
    .description = "unload/reload a command",
    .function = COMMAND_HANDLER_NAME(cmd),
    .valid_args = {
        .enabled = true,
        .counts = DynModule::craftArgCountMask<2>(),
        .split_type = DynModule::ValidArgs::Split::ByWhitespace,
        .usage = "/cmd <cmdname> <reload/unload>",
    }};
