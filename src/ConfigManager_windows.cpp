#include <ConfigManager.h>
#include <cstdlib>

using namespace ConfigManager;

void ConfigManager::setVariable(Configs config, const std::string &value) {
    _putenv("TOKEN=VAR_VALUE");
}
