#include <string.h>
#include <string>
#include <mutex>
#include <stdexcept>

#include <ConfigManager.h>

#include "popen_wdt.h"

// TODO Move this somewhere else
bool runCommand(const std::string& command, std::string& result) {
    auto fp = popen_watchdog(command.c_str(), nullptr);
    char buffer[512] = {0};
    if (fp) {
        while (fgets(buffer, sizeof(buffer), fp)) {
            result += buffer;
            memset(buffer, 0, sizeof(buffer));
        }
        if (result.back() == '\n')
            result.pop_back();
        return true;
    }
    return false;
}

std::string getSrcRoot() {
    static std::string dir;
    static std::once_flag flag;
    std::call_once(flag, [] {
        bool ret = ConfigManager::getVariable("SRC_ROOT", dir);
        if (!ret && !runCommand("git rev-parse --show-toplevel", dir)) {
            throw std::runtime_error("Command failed");
        }
    });
    return dir;
}
