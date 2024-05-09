#include <ConfigManager.h>

using namespace ConfigManager;

void ConfigManager::setVariable(Configs config, const std::string &value) {
#if _POSIX_C_SOURCE > 200112L || defined __APPLE__
    std::string var =
        ConfigManager::kConfigsMap.at(static_cast<int>(config)).second;
    setenv(var.c_str(), value.c_str(), 1);
#endif
}
