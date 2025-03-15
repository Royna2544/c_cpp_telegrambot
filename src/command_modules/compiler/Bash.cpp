#include "CompilerInTelegram.hpp"

void CompilerInTgForBash::run(MessageExt::Ptr message) {
    std::stringstream res;

    if (message->has<MessageAttrs::ExtraText>()) {
        runCommand(message->get<MessageAttrs::ExtraText>(), res, !allowhang);
        _callback->onResultReady(res.str());
    } else {
        _callback->onErrorStatus(absl::InvalidArgumentError(
            _locale->get(Strings::SEND_BASH_COMMAND).data()));
    }
}
