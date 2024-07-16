#include <StringResManager.hpp>

#include "TgBotWrapper.hpp"
#include "compiler/CompilerInTelegram.hpp"

static DECLARE_COMMAND_HANDLER(bash,, message) {
    static CompilerInTgForBashImpl bash(false);
    bash.run(message);
}
static DECLARE_COMMAND_HANDLER(ubash,, message) {
    static CompilerInTgForBashImpl ubash(true);
    ubash.run(message);
}

DYN_COMMAND_FN(name, module) {
    if (name == nullptr) {
        return false;
    }
    std::string commandName = name;
    module.command = commandName;
    module.isLoaded = true;
    module.flags = CommandModule::Flags::Enforced;
    if (commandName == "bash") {
        module.description = GETSTR(BASH_CMD_DESC);
        module.fn = COMMAND_HANDLER_NAME(bash);
    } else if (commandName == "ubash") {
        module.description = GETSTR(UBASH_CMD_DESC);
        module.fn = COMMAND_HANDLER_NAME(ubash);
    }
    return true;
}
