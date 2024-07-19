#include <StringResManager.hpp>
#include <TgBotWrapper.hpp>

#include "CompilerInTelegram.hpp"
#include "absl/status/status.h"

void CompilerInTgForBash::run(const Message::Ptr& message) {
    std::stringstream res;
    MessageWrapperLimited wrapper(message);

    if (wrapper.hasExtraText()) {
        runCommand(wrapper.getExtraText(), res, !allowhang);
        _interface->onResultReady(res.str());
    } else {
        _interface->onErrorStatus(
            absl::InvalidArgumentError(GETSTR(SEND_BASH_COMMAND)));
    }
}
