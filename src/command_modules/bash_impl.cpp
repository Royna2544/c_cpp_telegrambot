#include <StringResManager.hpp>

#include "TgBotWrapper.hpp"
#include "compiler/CompilerInTelegram.hpp"

static void BashCommandFn(const TgBotWrapper* wrapper, MessagePtr message) {
    static CompilerInTgForBashImpl bash(false);
    bash.run(message);
}
static void unsafeBashCommandFn(const TgBotWrapper* wrapper,
                                MessagePtr message) {
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
        module.fn = BashCommandFn;
    } else if (commandName == "ubash") {
        module.description = GETSTR(UBASH_CMD_DESC);
        module.fn = unsafeBashCommandFn;
    }
    return true;
}
