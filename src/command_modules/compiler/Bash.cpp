#include "CompilerInTelegram.hpp"
#include "TinyStatus.hpp"

void CompilerInTgForBash::run(MessageExt::Ptr message) {
    run(message, {});
}

void CompilerInTgForBash::run(MessageExt::Ptr message, std::stop_token stop) {
    std::stringstream res;

    if (message->has<MessageAttrs::ExtraText>()) {
        runCommand(message->get<MessageAttrs::ExtraText>(), res, !allowhang,
                   stop);
        _callback->onResultReady(res.str());
    } else {
        _callback->onErrorStatus(
            tinystatus::TinyStatus(tinystatus::Status::kInvalidArgument,
                                   _locale->get(Strings::SEND_BASH_COMMAND)));
    }
}
