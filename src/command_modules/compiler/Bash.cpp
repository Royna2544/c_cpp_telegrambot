#include "CompilerInTelegram.hpp"
#include <StringResManager.hpp>
#include <TgBotWrapper.hpp>

void CompilerInTgForBashImpl::onResultReady(const Message::Ptr& who,
                                            const std::string& text) {
    CompilerInTgHelper::onResultReady(who, text);
}
void CompilerInTgForBashImpl::onFailed(const Message::Ptr& who,
                                       const ErrorType e) {
    CompilerInTgHelper::onFailed(who, e);
}

void CompilerInTgForBash::run(const Message::Ptr &message) {
    std::stringstream res;
    MessageWrapperLimited wrapper(message);

    if (wrapper.hasExtraText()) {
        runCommand(message, wrapper.getExtraText(), res, !allowhang);
    } else {
        res << GETSTR(SEND_BASH_COMMAND);
    }
    onResultReady(message, res.str());
}
