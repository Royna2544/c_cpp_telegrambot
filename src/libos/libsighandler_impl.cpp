#include <Database.h>
#include <SingleThreadCtrl.h>

#include <mutex>

#include "libsighandler.h"

void defaultSignalHandler(int s) {
    static std::once_flag once;
    std::call_once(once, [s] { defaultCleanupFunction(); });
    std::exit(0);
};

void defaultCleanupFunction() {
    LOG(LogLevel::INFO, "Exiting");
    gSThreadManager.destroyManager();
    database::DBWrapper.save();
}