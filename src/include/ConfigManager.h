#pragma once

#include <string>

// Abstract manager for config loader
// Currently have two sources, env and file
namespace ConfigManager {

/**
 * load - call-once initializer
 */
void load(void);

/**
 * getVariable - fetch variable name from manager
 *
 * @param name variable name
 * @param outvalue variable to store the env variable
 * @return whether the manager was able to fetch the variable
 */
bool getVariable(const std::string& name, std::string& outvalue);

};  // namespace ConfigManager
