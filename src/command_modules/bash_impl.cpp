#include "compiler/CompilerInTelegram.h"

#include "CommandModule.h"

static void BashCommandFn(const Bot &bot, const Message::Ptr message) {
    static CompilerInTgForBashImpl bash(bot, false);
    bash.run(message);
}
static void unsafeBashCommandFn(const Bot &bot, const Message::Ptr message) {
    static CompilerInTgForBashImpl ubash(bot, true);
    ubash.run(message);
}

void loadcmd_bash(CommandModule &module) {
    module.command = "bash";
    module.description = "Run bash commands";
    module.flags = CommandModule::Flags::Enforced;
    module.fn = BashCommandFn;
}

void loadcmd_ubash(CommandModule &module) {
    module.command = "ubash";
    module.description = "Run bash commands notimeout";
    module.flags = CommandModule::Flags::Enforced;
    module.fn = unsafeBashCommandFn;
}
