#pragma once

#include <optional>
#include <string>

#include "CompileTimeStringConcat.hpp"
#include "EnumArrayHelpers.h"

// Abstract manager for config loader
// Currently have three sources, env and file, cmdline
namespace ConfigManager {

enum class Configs {
    TOKEN,
    SRC_ROOT,
    PATH,
    LOG_FILE,
    DATABASE_BACKEND,
    HELP,
    MAX
};

using ConfigStr = StringConcat::String<20>;
using DescStr = StringConcat::String<50>;

#define CONFIG_AND_STR(e)                                    \
    array_helpers::make_elem<Configs, ConfigStr>(Configs::e, \
                                                 StringConcat::cat(#e))

#define CONFIGALIAS_AND_STR(e, alias) \
    array_helpers::make_elem<Configs, const char>(Configs::e, alias)

#define DESC_AND_STR(e, desc)                              \
    array_helpers::make_elem<Configs, DescStr>(Configs::e, \
                                               StringConcat::cat(desc))

constexpr auto kConfigsMap =
    array_helpers::make<static_cast<int>(Configs::MAX), Configs, ConfigStr>(
        CONFIG_AND_STR(TOKEN), CONFIG_AND_STR(SRC_ROOT), CONFIG_AND_STR(PATH),
        CONFIG_AND_STR(LOG_FILE), CONFIG_AND_STR(DATABASE_BACKEND),
        CONFIG_AND_STR(HELP));

constexpr auto kConfigsAliasMap =
    array_helpers::make<static_cast<int>(Configs::MAX), Configs, const char>(
        CONFIGALIAS_AND_STR(TOKEN, 't'), CONFIGALIAS_AND_STR(SRC_ROOT, 'r'),
        CONFIGALIAS_AND_STR(PATH, 'p'), CONFIGALIAS_AND_STR(LOG_FILE, 'f'),
        CONFIGALIAS_AND_STR(DATABASE_BACKEND, 'd'),
        CONFIGALIAS_AND_STR(HELP, 'h'));

constexpr auto kConfigsDescMap =
    array_helpers::make<static_cast<int>(Configs::MAX), Configs, DescStr>(
        DESC_AND_STR(TOKEN, "Bot Token"),
        DESC_AND_STR(SRC_ROOT, "Root directory of source tree"),
        DESC_AND_STR(PATH, "Environment variable PATH (to override)"),
        DESC_AND_STR(LOG_FILE, "File path to log"),
        DESC_AND_STR(DATABASE_BACKEND, "Database backend to use"),
        DESC_AND_STR(HELP, "Print this help message"));

/**
 * getVariable - Function used to retrieve the value of a specific
 * configuration.
 *
 * @param config The configuration for which the value is to be retrieved.
 * @return A std::optional containing the value of the specified configuration,
 * or std::nullopt if the configuration is not found.
 */
std::optional<std::string> getVariable(Configs config);

/**
 * setVariable - Function used to set the value of a specific configuration.
 *
 * @param config The configuration for which the value is to be set.
 * @param value The new value to be set for the specified configuration.
 */
void setVariable(Configs config, const std::string &value);

/**
 * serializeHelpToOStream - Function used to serialize the help information to
 * an output stream.
 *
 * @param out The output stream to which the help information will be
 * serialized.
 */
void serializeHelpToOStream(std::ostream &out);

};  // namespace ConfigManager

enum class CommandLineOp { INSERT, GET };

/**
 * copyCommandLine - function used to obtain command line
 *  argument datas to command line backend
 * Basically just a container for argc, argv
 *
 * @param op Operation desired
 * @param out_argc [inout] argc, may be null.
 * @param out_argv [inout] argv, may be null.
 */
void copyCommandLine(CommandLineOp op, int *argc, char *const **argv);
