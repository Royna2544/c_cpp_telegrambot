#include <BotReplyMessage.h>
#include <NamespaceImport.h>

#include <functional>

using command_callback_t = std::function<void(const Bot&, const Message::Ptr&)>;

void bot_AddCommandPermissive(Bot& bot, const char* cmd, command_callback_t cb);
void bot_AddCommandEnforced(Bot& bot, const char* cmd, command_callback_t cb);

#define CMD_UNSUPPORTED(bot_, cmd, reason)                                              \
    bot_AddCommandEnforced(bot_, cmd, [](const Bot& bot, const Message::Ptr& message) { \
        bot_sendReplyMessage(                                                           \
            bot, message, "cmd '" cmd "' is unsupported.\nReason: " reason);            \
    });

#define NOT_SUPPORTED_DB(bot, name) \
    CMD_UNSUPPORTED(bot, name, "USE_DATABASE flag not enabled")
#define NOT_SUPPORTED_COMPILER(bot, name) \
    CMD_UNSUPPORTED(bot, name, "Host does not have corresponding compiler")
