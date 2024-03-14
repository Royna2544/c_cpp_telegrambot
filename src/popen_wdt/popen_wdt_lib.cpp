#include <ConfigManager.h>
#include <Logging.h>
#include <libos/libfs.hpp>

#include <cstring>
#include <mutex>
#include <stdexcept>

#include "popen_wdt.h"
#include "popen_wdt.hpp"

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
        if (result.back() == '\n') result.pop_back();

        LOG(LogLevel::VERBOSE, "Command: %s, result: '%s'", command.c_str(),
            result.c_str());
        return true;
    }
    return false;
}

std::filesystem::path getSrcRoot() {
    static std::filesystem::path dir;
    static std::once_flag flag;
    std::call_once(flag, [] {
        std::string dir_str;
        if (auto ret = ConfigManager::getVariable("SRC_ROOT");
            ret.has_value()) {
            dir_str = ret.value();
        } else if (!runCommand("git rev-parse --show-toplevel", dir_str)) {
            throw std::runtime_error("Command failed");
        }
        dir = dir_str;
        makeRelativeToCWD(dir);
    });
    return dir;
}
