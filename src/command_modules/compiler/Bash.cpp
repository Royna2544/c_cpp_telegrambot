#include "CompilerInTelegram.h"
#include <StringResManager.hpp>
#include <MessageWrapper.hpp>

void CompilerInTgForBashImpl::onResultReady(const Message::Ptr& who,
                                            const std::string& text) {
    CompilerInTgHelper::onResultReady(_bot, who, text);
}
void CompilerInTgForBashImpl::onFailed(const Message::Ptr& who,
                                       const ErrorType e) {
    CompilerInTgHelper::onFailed(_bot, who, e);
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
