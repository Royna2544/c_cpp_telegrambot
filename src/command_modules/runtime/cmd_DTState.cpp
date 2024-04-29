#include <BotReplyMessage.h>
#include <mutex>

#include "BotAddCommand.h"
#include "cmd_dynamic.h"
#include "command_modules/CommandModule.h"

static void cmd_DT(const Bot& bot, const Message::Ptr& message) {
    static std::once_flag once;
    static MessageId msgid = 0;
    static ChatId chatid = 0;
    static bool ok = false;
    std::call_once(once, [&bot, message]() {
        auto msg =
            bot_sendMessage(bot, message->chat->id, "DT State: Available");
        msgid = msg->messageId;
        chatid = msg->chat->id;
        bot.getApi().pinChatMessage(chatid, msgid);
        ok = true;
    });
    switch (ok) {
        case false:
            bot_editMessage(bot, chatid, msgid, "DT State: Available");
            break;
        case true:
            bot_editMessage(bot, chatid, msgid, "DT State: Unavailable");
            break;
    };
    ok = !ok;
}

extern "C" {
void DYN_COMMAND_SYM (CommandModule &module) {
    module.command = "dtstate";
    module.description = "Toggle DT State";
    module.fn = cmd_DT;
    module.flags = CommandModule::Flags::Enforced;
};
}