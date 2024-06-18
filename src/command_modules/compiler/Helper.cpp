#include <BotReplyMessage.h>
#include "CompilerInTelegram.h"
#include <absl/log/log.h>
#include <boost/algorithm/string/trim.hpp>

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
    std::string text_ = text;
    boost::trim(text_);
    bot_sendReplyMessage(bot, message, text_);
}

void CompilerInTgHelper::onCompilerPathCommand(const Bot &bot,
                                               const Message::Ptr &message,
                                               const std::string &text) {
    LOG(INFO) << text;
    bot_sendReplyMessage(bot, message, text);
}
