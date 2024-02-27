#include "CommandModule.h"
#include <CompilerInTelegram.h>

static void BashCommandFn(const Bot &bot, const Message::Ptr message) {
    CompileRunHandler(BashHandleData{{bot, message}, false});
}
static void unsafeBashCommandFn(const Bot &bot, const Message::Ptr message) {
    CompileRunHandler(BashHandleData{{bot, message}, true});
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