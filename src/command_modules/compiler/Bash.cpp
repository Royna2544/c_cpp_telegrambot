#include "CompilerInTelegram.h"
#include <ExtArgs.h>

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
    std::string cmd;

    if (hasExtArgs(message)) {
        parseExtArgs(message, cmd);
        runCommand(message, cmd, res, !allowhang);
    } else {
        res << "Send a bash command to run";
    }
    onResultReady(message, res.str());
}
