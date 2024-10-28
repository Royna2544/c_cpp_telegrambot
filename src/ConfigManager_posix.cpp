#include <ConfigManager.hpp>

void ConfigManager::set(Configs config, const std::string &value) {
#if _POSIX_C_SOURCE > 200112L || defined __APPLE__
    std::string var = kConfigsMap.at(static_cast<int>(config)).second;
    setenv(var.c_str(), value.c_str(), 1);
#endif
}

bool ConfigManager::getEnv(const std::string &name, std::string &value) {
    char *env = getenv(name.c_str());
    if (env == nullptr) {
        value.clear();
        return false;
    }
    value = env;
    return true;
}