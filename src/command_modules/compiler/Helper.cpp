#include "Helper.hpp"

#include <absl/log/check.h>
#include <absl/log/log.h>
#include <fmt/chrono.h>
#include <fmt/format.h>

#include <api/MessageExt.hpp>
#include <thread>
#include <utility>

#include "StringResLoader.hpp"

CompilerInTgBotInterface::CompilerInTgBotInterface(
    TgBotApi::CPtr api, const StringResLoaderBase::LocaleStrings* locale,
    MessageExt::Ptr requestedMessage)
    : botApi(api),
      requestedMessage(std::move(requestedMessage)),
      _locale(locale) {}

void CompilerInTgBotInterface::onExecutionStarted(
    const std::string_view& command) {
    timePoint.init();
    output << fmt::format("{}...\n{}: {}",
                          access(_locale, Strings::WORKING_ON_IT),
                          access(_locale, Strings::COMMAND_IS), command.data());
    sentMessage =
        botApi->sendReplyMessage(requestedMessage->message(), output.str());
}

void CompilerInTgBotInterface::onExecutionFinished(
    const std::string_view& command) {
    output << fmt::format("\n{} {}", access(_locale, Strings::DONE_TOOK),
                          timePoint.get());
    botApi->editMessage(sentMessage, output.str());
}

void CompilerInTgBotInterface::onErrorStatus(absl::Status status) {
    output << fmt::format("{} {}\n{}", access(_locale, Strings::ERROR_TOOK),
                          timePoint.get(), status.ToString());
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
        output << fmt::format("{}\n",
                              access(_locale, Strings::OUTPUT_IS_EMPTY));
        botApi->editMessage(sentMessage, output.str());
    }
}

void CompilerInTgBotInterface::onWdtTimeout() {
    output << "WDT TIMEOUT" << std::endl;
    botApi->editMessage(sentMessage, output.str());
    std::this_thread::sleep_for(1s);
}