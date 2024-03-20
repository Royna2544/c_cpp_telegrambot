#include <Authorization.h>
#include <BotAddCommand.h>
#include <ExtArgs.h>

static bool check(Bot& bot, const Message::Ptr& message) {
    static const std::string myName = bot.getApi().getMe()->username;

    std::string text = message->text;
    if (hasExtArgs(message)) {
        text = message->text.substr(0, firstBlank(message));
    }
    auto v = StringTools::split(text, '@');
    if (v.size() == 2 && v[1] != myName) return false;

    return true;
}

void bot_AddCommandPermissive(Bot& bot, const std::string& cmd,
                              command_callback_t cb) {
    auto authFn = [&, cb](const Message::Ptr message) {
        if (Authorized(message, AuthorizeFlags::PERMISSIVE |
                                    AuthorizeFlags::REQUIRE_USER) &&
            check(bot, message))
            cb(bot, message);
    };
    bot.getEvents().onCommand(cmd, authFn);
}

void bot_AddCommandEnforced(Bot& bot, const std::string& cmd,
                            command_callback_t cb) {
    auto authFn = [&, cb](const Message::Ptr message) {
        if (Authorized(message, AuthorizeFlags::REQUIRE_USER) &&
            check(bot, message))
            cb(bot, message);
    };
    bot.getEvents().onCommand(cmd, authFn);
}

void bot_AddCommandEnforcedMod(Bot& bot, const std::string& cmd,
                               command_callback_modbot_t cb) {
    auto authFn = [&, cb](const Message::Ptr message) {
        if (Authorized(message, AuthorizeFlags::REQUIRE_USER)) cb(bot, message);
    };
    bot.getEvents().onCommand(cmd, authFn);
}
