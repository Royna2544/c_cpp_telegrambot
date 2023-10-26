#include <BotReplyMessage.h>

#include "cmd_dynamic.h"

static void cmdTest(const Bot& bot, const Message::Ptr& message) {
    bot_sendReplyMessage(bot, message, "Hello world");
}

DECL_DYN_COMMAND(true, "test", cmdTest);
