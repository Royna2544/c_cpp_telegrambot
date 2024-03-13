#include <CompilerInTelegram.h>

#include "CommandModule.h"

static void BashCommandFn(const Bot &bot, const Message::Ptr message) {
    static CompilerInTgForBashImpl bash(bot, false);
    bash.run(message);
}
static void unsafeBashCommandFn(const Bot &bot, const Message::Ptr message) {
    static CompilerInTgForBashImpl ubash(bot, true);
    ubash.run(message);
}

struct CommandModule cmd_bash("bash", "Execute bash commands",
                              CommandModule::Flags::Enforced, BashCommandFn);

struct CommandModule cmd_ubash("ubash", "Execute bash commands (no timeout)",
                              CommandModule::Flags::Enforced, unsafeBashCommandFn);