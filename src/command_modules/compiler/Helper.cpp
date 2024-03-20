#include <BotReplyMessage.h>
#include "CompilerInTelegram.h"
#include <Logging.h>

void CompilerInTgHelper::onFailed(const Bot &bot, const Message::Ptr &message,
                                  const CompilerInTg::ErrorType e) {
    std::string text;
    switch (e) {
        case CompilerInTg::ErrorType::MESSAGE_VERIFICATION_FAILED:
            text = "Reply to a message with code as text";
            break;
        case CompilerInTg::ErrorType::FILE_WRITE_FAILED:
            text = "Failed to write output file";
            break;
        case CompilerInTg::ErrorType::POPEN_WDT_FAILED:
            text = "Failed to run command";
            break;
        case CompilerInTg::ErrorType::START_COMPILER:
            text = "Working on it...";
            break;
    };
    bot_sendReplyMessage(bot, message, text);
}

void CompilerInTgHelper::onResultReady(const Bot &bot,
                                       const Message::Ptr &message,
                                       const std::string &text) {
    bot_sendReplyMessage(bot, message, text);
}

void CompilerInTgHelper::onCompilerPathCommand(const Bot &bot,
                                               const Message::Ptr &message,
                                               const std::string &text) {
    LOG(LogLevel::DEBUG, "%s", text.c_str());
    bot_sendReplyMessage(bot, message, text);
}
