#include <ConfigManager.h>
#include <Logging.h>

#include <cstring>
#include <mutex>
#include <stdexcept>

#include "popen_wdt.hpp"
#include "popen_wdt.h"

// TODO Move this somewhere else
bool runCommand(const std::string& command, std::string& result) {
    auto fp = popen_watchdog(command.c_str(), nullptr);
    char buffer[512] = {0};

    if (fp) {
        result.clear();
        while (fgets(buffer, sizeof(buffer), fp)) {
            result += buffer;
            memset(buffer, 0, sizeof(buffer));
        }
        if (result.back() == '\n')
            result.pop_back();
        
        LOG_V("Command: %s, result: '%s'", command.c_str(), result.c_str());
        return true;
    }
    return false;
}

std::filesystem::path getSrcRoot() {
    static std::filesystem::path dir;
    static std::once_flag flag;
    std::call_once(flag, [] {
        std::string dir_str;
        bool ret = ConfigManager::getVariable("SRC_ROOT", dir_str);
        if (!ret && !runCommand("git rev-parse --show-toplevel", dir_str)) {
            throw std::runtime_error("Command failed");
        }
        dir = dir_str;
        dir.make_preferred();
    });
    return dir;
}
