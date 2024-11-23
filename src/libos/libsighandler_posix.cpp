#include <absl/log/log.h>

#include <array>
#include <csignal>

#include "libsighandler.hpp"

constexpr std::array<int, 2> kHandledSignals{SIGINT, SIGTERM};

void SignalHandler::install() {
    for (const auto &sig : kHandledSignals) {
        if (std::signal(sig, SignalHandler::signalHandler) == SIG_ERR) {
            PLOG(ERROR) << "Failed to install signal handler";
        }
    }
    // SIGUSR1 used to ignore thread's kills
    if (signal(SIGUSR1, SIG_IGN) == SIG_ERR) {
        PLOG(ERROR) << "Failed to ignore SIGUSR1";
    }
}

void SignalHandler::uninstall() {
    for (const auto &sig : kHandledSignals) {
        if (std::signal(sig, SIG_DFL) == SIG_ERR) {
            PLOG(ERROR) << "Failed to uninstall signal handler";
        }
    }
}