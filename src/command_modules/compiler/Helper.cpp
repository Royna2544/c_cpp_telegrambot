#include "Helper.hpp"

#include <absl/log/check.h>
#include <absl/log/log.h>

#include <StringResManager.hpp>
#include <TgBotWrapper.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <thread>
#include <utility>

CompilerInTgBotInterface::CompilerInTgBotInterface(
    std::shared_ptr<TgBotApi> api, Message::Ptr requestedMessage)
    : botApi(std::move(api)), requestedMessage(std::move(requestedMessage)) {}

void CompilerInTgBotInterface::onExecutionStarted(
    const std::string_view& command) {
    timePoint.init();
    sentMessage = botApi->sendReplyMessage(
        requestedMessage, GETSTR(WORKING) + command.data());
}

void CompilerInTgBotInterface::onExecutionFinished(
    const std::string_view& command) {
    const auto timePassed = timePoint.get();
    std::stringstream ss;
    ss << GETSTR(WORKING) + command.data() << std::endl;
    ss << "Done. Execution took " << timePassed.count() << " milliseconds";
    botApi->editMessage(sentMessage, ss.str());
}

void CompilerInTgBotInterface::onErrorStatus(absl::Status status) {
    std::stringstream ss;
    ss << "Error executing: " << status;
    if (sentMessage) {
        botApi->editMessage(sentMessage, ss.str());
    } else {
        botApi->sendReplyMessage(requestedMessage, ss.str());
    }
}

void CompilerInTgBotInterface::onResultReady(const std::string& text) {
    botApi->sendMessage(requestedMessage, text);
}

void CompilerInTgBotInterface::onWdtTimeout() {
    botApi->editMessage(sentMessage, "WDT TIMEOUT");
    std::this_thread::sleep_for(1s);
}