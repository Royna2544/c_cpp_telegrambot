#include "Helper.hpp"

#include <absl/log/check.h>
#include <absl/log/log.h>

#include <StringResManager.hpp>
#include <thread>
#include <utility>

#include "api/MessageExt.hpp"

CompilerInTgBotInterface::CompilerInTgBotInterface(
    InstanceClassBase<TgBotApi>::const_pointer_type api, MessageExt::Ptr requestedMessage)
    : botApi(api), requestedMessage(std::move(requestedMessage)) {}

void CompilerInTgBotInterface::onExecutionStarted(
    const std::string_view& command) {
    timePoint.init();
    output << GETSTR(WORKING) << command.data() << std::endl;
    sentMessage =
        botApi->sendReplyMessage(requestedMessage->message(), output.str());
}

void CompilerInTgBotInterface::onExecutionFinished(
    const std::string_view& command) {
    const auto timePassed = timePoint.get();
    output << "Done. Execution took " << timePassed.count() << " milliseconds"
           << std::endl;
    botApi->editMessage(sentMessage, output.str());
}

void CompilerInTgBotInterface::onErrorStatus(absl::Status status) {
    output << "Error executing: " << status;
    if (sentMessage) {
        botApi->editMessage(sentMessage, output.str());
    } else {
        botApi->sendReplyMessage(requestedMessage->message(), output.str());
    }
}

void CompilerInTgBotInterface::onResultReady(const std::string& text) {
    if (!text.empty()) {
        botApi->sendMessage(requestedMessage->get<MessageAttrs::Chat>(), text);
    } else {
        output << "Output is empty" << std::endl;
        botApi->editMessage(sentMessage, output.str());
    }
}

void CompilerInTgBotInterface::onWdtTimeout() {
    output << "WDT TIMEOUT" << std::endl;
    botApi->editMessage(sentMessage, output.str());
    std::this_thread::sleep_for(1s);
}