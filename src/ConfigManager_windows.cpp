#include <ConfigManager.h>
#include <windows.h>

#include "CStringLifetime.h"

using namespace ConfigManager;

void ConfigManager::setVariable(Configs config, const std::string &value_) {
    CStringLifetime key =
        ConfigManager::kConfigsMap.at(static_cast<int>(config)).second;
    CStringLifetime value = value_;
    SetEnvironmentVariable(key, value);
}

bool ConfigManager::getEnv(const std::string &name, std::string &value) {
    CStringLifetime key = name;
    std::array<char, 1024> buf = {};
    if (GetEnvironmentVariable(key, buf.data(), buf.size()) != 0) {
        value = buf.data();
        return true;
    }
    return false;
}