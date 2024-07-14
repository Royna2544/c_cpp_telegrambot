#include "OnTerminateRegistrar.hpp"
#include "libsighandler.hpp"
#include <absl/log/log.h>

std::atomic_bool SignalHandler::kUnderSignal;

void SignalHandler::signalHandler() {
    LOG(INFO) << "Received signal...";
    kUnderSignal = true;
}

DECLARE_CLASS_INST(OnTerminateRegistrar);