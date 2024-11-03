#include <ConfigManager.hpp>
#include <Windows.h>

bool ConfigManager::getEnv(const std::string_view name, std::string &value) {
    std::array<char, 1024> buf = {};
    if (GetEnvironmentVariable(name.data(), buf.data(), buf.size()) != 0) {
        value = buf.data();
        return true;
    }
    return false;
}