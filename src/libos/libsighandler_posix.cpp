#include <array>
#include <csignal>
#include <absl/log/log.h>

#include "libsighandler.hpp"

constexpr std::array<int, 2> kHandledSignals {
    SIGINT, SIGTERM
};

void SignalHandler::install() {
    for (const auto &sig : kHandledSignals) {
        if (std::signal(sig, SignalHandler::signalHandler) == SIG_ERR) {
            PLOG(ERROR) << "Failed to install signal handler";
        }
    }
}

void SignalHandler::uninstall() {
    for (const auto &sig : kHandledSignals) {
        if (std::signal(sig, SIG_DFL) == SIG_ERR) {
            PLOG(ERROR) << "Failed to uninstall signal handler";
        }
    }
}