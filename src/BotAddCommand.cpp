#include <Authorization.h>
#include <BotAddCommand.h>

void bot_AddCommandPermissive(Bot& bot, const std::string& cmd, command_callback_t cb) {
    auto authFn = [&, cb](const Message::Ptr message) {
        if (Authorized(message, AuthorizeFlags::PERMISSIVE |
                                    AuthorizeFlags::REQUIRE_USER))
            cb(bot, message);
    };
    bot.getEvents().onCommand(cmd, authFn);
}

void bot_AddCommandEnforced(Bot& bot, const std::string& cmd, command_callback_t cb) {
    auto authFn = [&, cb](const Message::Ptr message) {
        if (Authorized(message, AuthorizeFlags::REQUIRE_USER)) cb(bot, message);
    };
    bot.getEvents().onCommand(cmd, authFn);
}
