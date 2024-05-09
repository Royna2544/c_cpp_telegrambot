#include <ConfigManager.h>
#include <cstdlib>

using namespace ConfigManager;

void ConfigManager::setVariable(Configs config, const std::string &value) {
    std::string string;
    string += ConfigManager::kConfigsMap.at(static_cast<int>(config)).second;
    string += '=';
    string += value;
    _putenv(string.c_str());
}
