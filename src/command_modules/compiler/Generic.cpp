#include <api/StringResLoader.hpp>
#include <filesystem>
#include <fstream>

#include "CompilerInTelegram.hpp"
#include "TinyStatus.hpp"

// Verify, Parse, Write
bool CompilerInTgForGeneric::verifyParseWrite(const MessageExt::Ptr& message,
                                              std::string& extraargs) {
    if (!message->reply()->has<MessageAttrs::ExtraText>()) {
        _callback->onErrorStatus(
            tinystatus::TinyStatus(tinystatus::Status::kInvalidArgument,
                                   _locale->get(Strings::REPLY_TO_A_CODE)));
        return false;
    }
    if (message->has<MessageAttrs::ExtraText>()) {
        extraargs = message->get<MessageAttrs::ExtraText>();
    }
    std::ofstream file(params.outfile);
    if (file.fail()) {
        _callback->onErrorStatus(tinystatus::TinyStatus(
            tinystatus::Status::kWriteError,
            _locale->get(Strings::FAILED_TO_WRITE_FILE)));
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
