#include <ConfigManager.h>

using namespace ConfigManager;

void ConfigManager::setVariable(Configs config, const std::string &value) {
#if _POSIX_C_SOURCE > 200112L || defined __APPLE__
    // Set up the mock environment variables
    setenv("TOKEN", "VAR_VALUE", 1);
#endif
}
