#include <absl/log/log.h>

#include <ManagedThreads.hpp>
#include <mutex>

#include "InstanceClassBase.hpp"
#include "OnTerminateRegistrar.hpp"
#include "libsighandler.h"

void defaultSignalHandler(int s) {
    static std::once_flag once;
    std::call_once(once, [s] { defaultCleanupFunction(s); });
    std::exit(0);
};

void defaultCleanupFunction(int bySignal) {
    LOG(INFO) << "Exiting";
    OnTerminateRegistrar::getInstance()->callCallbacks(bySignal);
    LOG(INFO) << "TgBot process exiting, Goodbye!";
}

DECLARE_CLASS_INST(OnTerminateRegistrar);