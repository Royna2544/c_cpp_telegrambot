#include <Authorization.h>
#include <BotAddCommand.h>

void bot_AddCommandPermissive(Bot& bot, const char* cmd, command_callback_t cb) {
    auto authFn = [&](const Message::Ptr message) {
        if (!Authorized(message, true, true)) return;
        cb(bot, message);
    };
    bot.getEvents().onCommand(cmd, authFn);
}

void bot_AddCommandEnforced(Bot& bot, const char* cmd, command_callback_t cb) {
    auto authFn = [&](const Message::Ptr message) {
        if (!Authorized(message)) return;
        cb(bot, message);
    };
    bot.getEvents().onCommand(cmd, authFn);
}
