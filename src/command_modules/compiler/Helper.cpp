#include "Helper.hpp"

#include <absl/log/check.h>
#include <absl/log/log.h>
#include <fmt/chrono.h>
#include <fmt/format.h>

#include <api/MessageExt.hpp>
#include <string_view>
#include <thread>
#include <utility>

#include "StringResLoader.hpp"
#include "popen_wdt.h"

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
    const std::string_view& command, const popen_watchdog_exit_t& exit) {
    const std::string_view type = exit.signal ? "signal" : "exit";
    output << fmt::format("\n{} {}\n{} {} {}",
                          access(_locale, Strings::DONE_TOOK), timePoint.get(),
                          access(_locale, Strings::PROCESS_EXITED), type,
                          exit.exitcode);
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
        output << fmt::format("\n{}",
                              access(_locale, Strings::OUTPUT_IS_EMPTY));
        botApi->editMessage(sentMessage, output.str());
    }
}

void CompilerInTgBotInterface::onWdtTimeout() {
    output << "\nWDT TIMEOUT" << std::endl;
    botApi->editMessage(sentMessage, output.str());
}