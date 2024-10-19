#include <StringResManager.hpp>

#include "CompilerInTelegram.hpp"

void CompilerInTgForBash::run(MessageExt::Ptr message) {
    std::stringstream res;

    if (message->has<MessageAttrs::ExtraText>()) {
        runCommand(message->get<MessageAttrs::ExtraText>(), res, !allowhang);
        _interface->onResultReady(res.str());
    } else {
        _interface->onErrorStatus(
            absl::InvalidArgumentError(GETSTR(SEND_BASH_COMMAND)));
    }
}
