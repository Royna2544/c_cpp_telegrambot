#include <BotReplyMessage.h>
#include <mutex>

#include "cmd_dynamic.h"

static void cmd_DT(const Bot& bot, const Message::Ptr& message) {
    static std::once_flag once;
    static MessageId msgid = 0;
    static ChatId chatid = 0;
    static bool ok = false;
    std::call_once(once, [&bot, message]() {
        auto msg =
            bot_sendMessage(bot, message->from->id, "DT State: Available");
        msgid = msg->messageId;
        chatid = msg->chat->id;
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
struct dynamicCommandModule DYN_COMMAND_SYM {
    .mod = CommandModule("newdtstate", "Update DT state",
                         CommandModule::Flags::Enforced, cmd_DT),
    .isSupported = nullptr,
};
}