#include <absl/status/status.h>

#include <TgBotWrapper.hpp>
#include <filesystem>
#include <fstream>

#include "CompilerInTelegram.hpp"

// Verify, Parse, Write
bool CompilerInTgForGeneric::verifyParseWrite(MessagePtr message,
                                              std::string& extraargs) {
    if (!message->has<MessageExt::Attrs::IsReplyMessage>()) {
        _interface->onErrorStatus(
            absl::InvalidArgumentError("Need a replied-to message"));
        return false;
    }
    if (message->has<MessageExt::Attrs::ExtraText>()) {
        extraargs = message->get<MessageExt::Attrs::ExtraText>();
    }
    if (message->replyToMessage->text.empty()) {
        _interface->onErrorStatus(
            absl::InvalidArgumentError("Reply must contain a non-empty text"));
        return false;
    }
    std::ofstream file(params.outfile);
    if (file.fail()) {
        _interface->onErrorStatus(
            absl::InternalError("Unable to write out file"));
        return false;
    }
    file << message->replyToMessage->text;
    file.close();
    return true;
}

void CompilerInTgForGeneric::run(MessagePtr message) {
    std::string extargs;
    std::stringstream cmd;
    std::stringstream res;

    if (verifyParseWrite(message, extargs)) {
        cmd << params.exe.string() << SPACE << extargs << SPACE
            << params.outfile.string();
        runCommand(cmd.str(), res);
        _interface->onResultReady(res.str());
        std::filesystem::remove(params.outfile);
    }
}
