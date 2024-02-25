#pragma once

#include <string>

// Abstract manager for config loader
// Currently have two sources, env and file
namespace ConfigManager {

/**
 * getVariable - fetch variable name from manager
 *
 * @param name variable name
 * @param outvalue variable to store the env variable
 * @return whether the manager was able to fetch the variable
 */
bool getVariable(const std::string& name, std::string& outvalue);

void printHelp();

};  // namespace ConfigManager

/**
 * copyCommandLine - function used to obtain command line
 *  argument datas to command line backend
 * Basically just a container for argc, argv
 *
 * @param argc [in] argc
 * @param argv [in] argv
 * @param out_argc [out] argc, may be null.
 * @param out_argv [out] argv, may be null.
 */
void copyCommandLine(const int argc, const char **argv, int *argc_out, const char ***argv_out);
