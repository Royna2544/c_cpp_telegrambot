#include <StringResManager.hpp>
#include <TgBotWrapper.hpp>

#include "CompilerInTelegram.hpp"

void CompilerInTgForBash::run(const Message::Ptr& message) {
    std::stringstream res;
    MessageWrapperLimited wrapper(message);

    if (wrapper.hasExtraText()) {
        runCommand(wrapper.getExtraText(), res, !allowhang);
    } else {
        res << GETSTR(SEND_BASH_COMMAND);
    }
    _interface->onResultReady(res.str());
}
