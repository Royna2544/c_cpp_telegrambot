#include <Authorization.h>
#include <BotAddCommand.h>
#include <ExtArgs.h>

#include <utility>

#include "InstanceClassBase.hpp"
#include "OnAnyMessageRegister.hpp"

// TODO Move this somewhere else
DECLARE_CLASS_INST(OnAnyMessageRegisterer);

static bool check(Bot& bot, const Message::Ptr& message,
                  unsigned int authflags) {
    static const std::string myName = bot.getApi().getMe()->username;

    std::string text = message->text;
    if (hasExtArgs(message)) {
        text = message->text.substr(0, firstBlank(message));
    }
    auto v = StringTools::split(text, '@');
    if (v.size() == 2 && v[1] != myName) {
        return false;
    }

    return AuthContext::getInstance()->isAuthorized(message, authflags);
}

static void Stub(const Message::Ptr& message) {}

void bot_AddCommand(Bot& bot, const std::string& cmd, command_callback_t cb,
                    bool enforced) {
    unsigned int authflags = AuthContext::Flags::REQUIRE_USER;
    if (!enforced) {
        authflags |= AuthContext::Flags::PERMISSIVE;
    }

    auto authFn = [&, cb = std::move(cb)](const Message::Ptr& message) {
        static const std::string myName = bot.getApi().getMe()->username;

        std::string text = message->text;
        if (hasExtArgs(message)) {
            text = message->text.substr(0, firstBlank(message));
        }
        auto v = StringTools::split(text, '@');
        if (v.size() == 2 && v[1] != myName) {
            return;
        }

        if (AuthContext::getInstance()->isAuthorized(message, authflags)) {
            cb(bot, message);
        }
    };
    bot.getEvents().onCommand(cmd, authFn);
}

void bot_RemoveCommand(Bot& bot, const std::string& cmd) {
    bot.getEvents().onCommand(cmd, Stub);
}