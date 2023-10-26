#include <BotReplyMessage.h>

#include "cmd_dynamic.h"

static void cmdTest(const Bot& bot, const Message::Ptr& message) {
    bot_sendReplyMessage(bot, message, "Hello world");
}

DECL_DYN_ENFORCED_COMMAND("test", cmdTest);
