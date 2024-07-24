#include <absl/log/log.h>

#include "OnTerminateRegistrar.hpp"
#include "libsighandler.hpp"

std::atomic_bool SignalHandler::kUnderSignal;

void SignalHandler::signalHandler() {
    if (!kUnderSignal) {
        LOG(INFO) << "Received signal...";
        kUnderSignal = true;
    }
}

DECLARE_CLASS_INST(OnTerminateRegistrar);