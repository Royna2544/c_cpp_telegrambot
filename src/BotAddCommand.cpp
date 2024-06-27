#include <Authorization.h>
#include <BotAddCommand.h>

#include <InstanceClassBase.hpp>
#include <MessageWrapper.hpp>
#include <OnAnyMessageRegister.hpp>
#include <algorithm>
#include <boost/algorithm/string/trim.hpp>
#include <utility>

// TODO Move this somewhere else
DECLARE_CLASS_INST(OnAnyMessageRegisterer);

static void Stub(const Message::Ptr& message) {}

void bot_AddCommand(Bot& bot, const std::string& cmd, command_callback_t cb,
                    bool enforced) {
    unsigned int authflags = AuthContext::Flags::REQUIRE_USER;
    if (!enforced) {
        authflags |= AuthContext::Flags::PERMISSIVE;
    }

    auto authFn = [&, authflags,
                   cb = std::move(cb)](const Message::Ptr& message) {
        static const std::string myName = bot.getApi().getMe()->username;
        MessageWrapperLimited wrapper(message);

        std::string text = message->text;
        if (wrapper.hasExtraText()) {
            text = text.substr(0, text.size() - wrapper.getExtraText().size());
            boost::trim(text);
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