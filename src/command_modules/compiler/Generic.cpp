#include <absl/status/status.h>

#include <filesystem>
#include <fstream>

#include "CompilerInTelegram.hpp"
#include <api/StringResLoader.hpp>

// Verify, Parse, Write
bool CompilerInTgForGeneric::verifyParseWrite(const MessageExt::Ptr& message,
                                              std::string& extraargs) {
    if (!message->reply()->has<MessageAttrs::ExtraText>()) {
        _callback->onErrorStatus(absl::InvalidArgumentError(
            _locale->get(Strings::REPLY_TO_A_CODE).data()));
        return false;
    }
    if (message->has<MessageAttrs::ExtraText>()) {
        extraargs = message->get<MessageAttrs::ExtraText>();
    }
    std::ofstream file(params.outfile);
    if (file.fail()) {
        _callback->onErrorStatus(absl::InternalError(
            _locale->get(Strings::FAILED_TO_WRITE_FILE).data()));
        return false;
    }
    file << message->reply()->get<MessageAttrs::ExtraText>();
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
        _callback->onResultReady(res.str());
        std::filesystem::remove(params.outfile);
    }
}
