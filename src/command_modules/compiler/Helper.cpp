#include <absl/log/log.h>

#include <StringResManager.hpp>
#include <TgBotWrapper.hpp>
#include <boost/algorithm/string/trim.hpp>

#include "CompilerInTelegram.hpp"

void CompilerInTgHelper::onFailed(TgBotApi *botApi, const Message::Ptr &message,
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
    botApi->sendReplyMessage(message, text);
}

void CompilerInTgHelper::onResultReady(TgBotApi *botApi,
                                       const Message::Ptr &message,
                                       const std::string &text) {
    std::string text_ = text;
    boost::trim(text_);
    botApi->sendReplyMessage(message, text_);
}

void CompilerInTgHelper::onCompilerPathCommand(TgBotApi *botApi,
                                               const Message::Ptr &message,
                                               const std::string &text) {
    LOG(INFO) << text;
    botApi->sendReplyMessage(message, text);
}
