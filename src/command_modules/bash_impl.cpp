#include "CommandModule.h"
#include <CompilerInTelegram.h>

static void BashCommandFn(const Bot &bot, const Message::Ptr message) {
    BashHandleData(bot, message, false).run();
}
static void unsafeBashCommandFn(const Bot &bot, const Message::Ptr message) {
    BashHandleData(bot, message, true).run();
}

struct CommandModule cmd_bash {
    .enforced = true,
    .name = "bash",
    .fn = BashCommandFn,
};

struct CommandModule cmd_unsafebash {
    .enforced = true,
    .name = "unsafebash",
    .fn = unsafeBashCommandFn,
};