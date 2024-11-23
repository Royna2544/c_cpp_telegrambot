#include <absl/log/log.h>

#include "libsighandler.hpp"

std::atomic_bool SignalHandler::kUnderSignal;

void SignalHandler::signalHandler() {
    if (!kUnderSignal) {
        kUnderSignal = true;
    }
}