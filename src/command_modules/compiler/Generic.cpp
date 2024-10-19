#include <absl/status/status.h>

#include <filesystem>
#include <fstream>

#include "CompilerInTelegram.hpp"

// Verify, Parse, Write
bool CompilerInTgForGeneric::verifyParseWrite(const MessageExt::Ptr& message,
                                              std::string& extraargs) {
    if (!message->replyMessage()->has<MessageAttrs::ExtraText>()) {
        _interface->onErrorStatus(absl::InvalidArgumentError(
            "Need a replied-to message with a non-empty text"));
        return false;
    }
    if (message->has<MessageAttrs::ExtraText>()) {
        extraargs = message->get<MessageAttrs::ExtraText>();
    }
    std::ofstream file(params.outfile);
    if (file.fail()) {
        _interface->onErrorStatus(
            absl::InternalError("Unable to write out file"));
        return false;
    }
    file << message->replyMessage()->get<MessageAttrs::ExtraText>();
    file.close();
    return true;
}

void CompilerInTgForGeneric::run(MessageExt::Ptr message) {
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
