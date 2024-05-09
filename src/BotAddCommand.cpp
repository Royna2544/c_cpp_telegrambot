#include <Authorization.h>
#include <BotAddCommand.h>
#include <ExtArgs.h>

#include "InstanceClassBase.hpp"
#include "OnAnyMessageRegister.hpp"

// TODO Move this somewhere else
DECLARE_CLASS_INST(OnAnyMessageRegisterer);

static bool check(Bot& bot, const Message::Ptr& message, int authflags) {
    static const std::string myName = bot.getApi().getMe()->username;

    std::string text = message->text;
    if (hasExtArgs(message)) {
        text = message->text.substr(0, firstBlank(message));
    }
    auto v = StringTools::split(text, '@');
    if (v.size() == 2 && v[1] != myName) return false;

    return AuthContext::getInstance()->isAuthorized(message, authflags);
}

void bot_AddCommandPermissive(Bot& bot, const std::string& cmd,
                              command_callback_t cb) {
    auto authFn = [&, cb](const Message::Ptr message) {
        if (check(bot, message,
                  AuthContext::Flags::PERMISSIVE |
                      AuthContext::Flags::REQUIRE_USER))
            cb(bot, message);
    };
    bot.getEvents().onCommand(cmd, authFn);
}

void bot_AddCommandEnforced(Bot& bot, const std::string& cmd,
                            command_callback_t cb) {
    auto authFn = [&, cb](const Message::Ptr message) {
        if (check(bot, message, AuthContext::Flags::REQUIRE_USER))
            cb(bot, message);
    };
    bot.getEvents().onCommand(cmd, authFn);
}