#include <BotReplyMessage.h>
#include "CompilerInTelegram.h"
#include <absl/log/log.h>
#include <boost/algorithm/string/trim.hpp>
#include <StringResManager.hpp>

void CompilerInTgHelper::onFailed(const Bot &bot, const Message::Ptr &message,
                                  const CompilerInTg::ErrorType e) {
    std::string text;
    switch (e) {
        case CompilerInTg::ErrorType::MESSAGE_VERIFICATION_FAILED:
            text = GETSTR(REPLY_TO_A_CODE);
            break;
        case CompilerInTg::ErrorType::FILE_WRITE_FAILED:
            text = GETSTR(FAILED_TO_WRITE_FILE);
            break;
        case CompilerInTg::ErrorType::POPEN_WDT_FAILED:
            text = GETSTR(FAILED_TO_RUN_COMMAND);
            break;
        case CompilerInTg::ErrorType::START_COMPILER:
            text = GETSTR(WORKING);
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
