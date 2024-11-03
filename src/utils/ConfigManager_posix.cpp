#include <ConfigManager.hpp>

bool ConfigManager::getEnv(const std::string_view name, std::string &value) {
    char *env = getenv(name.data());
    if (env == nullptr) {
        value.clear();
        return false;
    }
    value = env;
    return true;
}