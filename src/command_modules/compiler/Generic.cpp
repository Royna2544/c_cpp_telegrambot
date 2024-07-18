#include <TgBotWrapper.hpp>
#include <filesystem>

#include "CompilerInTelegram.hpp"
#include "absl/status/status.h"

// Verify, Parse, Write
bool CompilerInTgForGeneric::verifyParseWrite(const Message::Ptr& message,
                                              std::string& extraargs) {
    MessageWrapperLimited wrapper(message);
    if (wrapper.hasExtraText()) {
        extraargs = wrapper.getExtraText();
    }
    if (!wrapper.hasReplyToMessage()) {
        _interface->onErrorStatus(
            absl::InvalidArgumentError("Need a replied-to message"));
        return false;
    }
    wrapper.switchToReplyToMessage();
    if (wrapper.getText().empty()) {
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
    file << wrapper.getText();
    file.close();
    return true;
}

void CompilerInTgForGeneric::run(const Message::Ptr& message) {
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
