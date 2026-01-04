#include "Helper.hpp"

#include <absl/log/check.h>
#include <absl/log/log.h>
#include <fmt/chrono.h>
#include <fmt/format.h>

#include <api/MessageExt.hpp>
#include <api/StringResLoader.hpp>
#include <string_view>

#include "popen_wdt.h"

CompilerInTgBotInterface::CompilerInTgBotInterface(
    TgBotApi::CPtr api, const StringResLoader::PerLocaleMap* locale,
    MessageExt::Ptr requestedMessage)
    : botApi(api), requestedMessage(requestedMessage), _locale(locale) {}

void CompilerInTgBotInterface::onExecutionStarted(
    const std::string_view& command) {
    timePoint.init();
    output << fmt::format("{}...\n{}: {}", _locale->get(Strings::WORKING_ON_IT),
                          _locale->get(Strings::COMMAND_IS), command.data());
    sentMessage =
        botApi->sendReplyMessage(requestedMessage->message(), output.str());
}

void CompilerInTgBotInterface::onExecutionFinished(
    const std::string_view& /*command*/, const popen_watchdog_exit_t& exit) {
    const std::string_view type = exit.signal ? "signal" : "exit";
    output << fmt::format("\n{} {}\n{} {} {}\n",
                          _locale->get(Strings::DONE_TOOK), timePoint.get(),
                          _locale->get(Strings::PROCESS_EXITED), type,
                          exit.exitcode);
    botApi->editMessage(sentMessage, output.str());
}

void CompilerInTgBotInterface::onErrorStatus(TinyStatus status) {
    output << fmt::format("{} {}\n{}", _locale->get(Strings::ERROR_TOOK),
                          timePoint.get(), status.getMessage());
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
        output << fmt::format("\n{}", _locale->get(Strings::OUTPUT_IS_EMPTY));
        botApi->editMessage(sentMessage, output.str());
    }
}

void CompilerInTgBotInterface::onWdtTimeout() {
    output << "\nWDT TIMEOUT" << std::endl;
    botApi->editMessage(sentMessage, output.str());
}
