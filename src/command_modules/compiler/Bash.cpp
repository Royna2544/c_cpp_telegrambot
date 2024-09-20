#include <StringResManager.hpp>
#include <TgBotWrapper.hpp>

#include "CompilerInTelegram.hpp"
#include "absl/status/status.h"

void CompilerInTgForBash::run(const MessagePtr message) {
    std::stringstream res;

    if (message->has<MessageExt::Attrs::ExtraText>()) {
        runCommand(message->get<MessageExt::Attrs::ExtraText>(), res,
                   !allowhang);
        _interface->onResultReady(res.str());
    } else {
        _interface->onErrorStatus(
            absl::InvalidArgumentError(GETSTR(SEND_BASH_COMMAND)));
    }
}
