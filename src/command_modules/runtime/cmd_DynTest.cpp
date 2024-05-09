#include <BotReplyMessage.h>

#include "cmd_dynamic.h"
#include "command_modules/CommandModule.h"

static void cmdTest(const Bot& bot, const Message::Ptr& message) {
    bot_sendReplyMessage(bot, message, "Hello world");
}

static bool isSupported(void) { return false; }

extern "C" {
void DYN_COMMAND_SYM (CommandModule &module) {
    module.command = "test";
    module.description = "Test module to test RTLoader";
    module.fn = cmdTest;
    module.flags = CommandModule::Flags::Enforced;
    throw unsupported_command_error("Test module");
};
}