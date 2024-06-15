#include <SingleThreadCtrl.h>

#include <mutex>

#include "libsighandler.h"

void defaultSignalHandler(int s) {
    static std::once_flag once;
    std::call_once(once, [s] { defaultCleanupFunction(); });
    std::exit(0);
};

void defaultCleanupFunction() {
    LOG(INFO) << "Exiting";
    SingleThreadCtrlManager::getInstance()->destroyManager();
    LOG(INFO) << "TgBot process exiting, Goodbye!";
}