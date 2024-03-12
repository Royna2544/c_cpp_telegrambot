#include <BotReplyMessage.h>

#include "cmd_dynamic.h"
#include "command_modules/CommandModule.h"

static void cmdTest(const Bot& bot, const Message::Ptr& message) {
    bot_sendReplyMessage(bot, message, "Hello world");
}

static bool isSupported(void) { return false; }

extern "C" {
struct dynamicCommandModule DYN_COMMAND_SYM {
    .mod = CommandModule("test", "Test Command", CommandModule::Flags::Enforced,
                         cmdTest),
    .isSupported = isSupported,
};
}