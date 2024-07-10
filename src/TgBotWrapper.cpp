#include <TgBotWrapper.hpp>
#include "tgbot/types/Message.h"

void TgBotWrapper::addCommand(const std::string& cmd,
                              command_callback_t callback, bool enforced) {
    unsigned int authflags = AuthContext::Flags::REQUIRE_USER;
    if (!enforced) {
        authflags |= AuthContext::Flags::PERMISSIVE;
    }
    getEvents().onCommand(cmd, [&, authflags, callback = std::move(callback)](
                                   const Message::Ptr& message) {
        static const std::string myName = getBotUser()->username;
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
            callback(message);
        }
    });
}

void TgBotWrapper::removeCommand(const std::string& cmd) {
    getEvents().onCommand(cmd, {});
}

MessageWrapper TgBotWrapper::getMessageWrapper(const Message::Ptr& msg) {
    return MessageWrapper(msg);
}

MessageWrapperLimited TgBotWrapper::getMessageWrapperLimited(const Message::Ptr& msg) {
    return MessageWrapperLimited(msg);
}

bool TgBotWrapper::setBotCommands(const std::vector<BotCommand::Ptr>& commands) const{
    return getApi().setMyCommands(commands);
}