#pragma once

#include <optional>
#include <string>

// Abstract manager for config loader
// Currently have three sources, env and file, cmdline
namespace ConfigManager {

/**
 * getVariable - fetch variable name from manager
 *
 * @param name variable name
 * @param outvalue variable to store the env variable
 * @return whether the manager was able to fetch the variable
 */
std::optional<std::string> getVariable(const std::string &name);

void printHelp();

};  // namespace ConfigManager

enum CommandLineOp { INSERT, GET };

/**
 * copyCommandLine - function used to obtain command line
 *  argument datas to command line backend
 * Basically just a container for argc, argv
 *
 * @param op Operation desired
 * @param out_argc [inout] argc, may be null.
 * @param out_argv [inout] argv, may be null.
 */
void copyCommandLine(CommandLineOp op, int *argc, const char ***argv);
