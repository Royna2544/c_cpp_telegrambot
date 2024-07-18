#include <TgBotWrapper.hpp>
#include <filesystem>

#include "CompilerInTelegram.hpp"

// Verify, Parse, Write
bool CompilerInTgForGeneric::verifyParseWrite(const Message::Ptr& message,
                                              std::string& extraargs) {
    MessageWrapperLimited wrapper(message);
    if (wrapper.hasExtraText()) {
        extraargs = wrapper.getExtraText();
    }
    if (message->replyToMessage && !message->replyToMessage->text.empty()) {
        std::ofstream file(outfile);
        if (file.fail()) {
            onFailed(message, ErrorType::FILE_WRITE_FAILED);
            return false;
        }
        file << message->replyToMessage->text;
        file.close();
        return true;
    }
    onFailed(message, ErrorType::MESSAGE_VERIFICATION_FAILED);
    return false;
}

void CompilerInTgForGenericImpl::onResultReady(const Message::Ptr& message,
                                               const std::string& text) {
    CompilerInTgHelper::onResultReady(botApi, message, text);
}

void CompilerInTgForGenericImpl::onFailed(const Message::Ptr& who,
                                          const ErrorType e) {
    CompilerInTgHelper::onFailed(botApi, who, e);
}

void CompilerInTgForGeneric::run(const Message::Ptr& message) {
    std::string extargs;
    std::stringstream cmd, res;

    if (verifyParseWrite(message, outfile)) {
        cmd << cmdPrefix.string() << SPACE << outfile;
        runCommand(message, cmd.str(), res);
        onResultReady(message, res.str());
        std::filesystem::remove(outfile);
    }
}
