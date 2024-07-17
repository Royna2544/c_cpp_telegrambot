#pragma once

#include <TgBotUtilsExports.h>

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
    OVERRIDE_CONF,
    SOCKET_BACKEND,
    SELECTOR,
    LOCALE,
    MAX
};

using ConfigStr = StringConcat::String<20>;
using DescStr = StringConcat::String<50>;

#define CONFIG_AND_STR(e)                         \
    array_helpers::make_elem<Configs, ConfigStr>( \
        Configs::e, StringConcat::createString(#e))

#define CONFIGALIAS_AND_STR(e, alias) \
    array_helpers::make_elem<Configs, const char>(Configs::e, alias)

#define DESC_AND_STR(e, desc)                   \
    array_helpers::make_elem<Configs, DescStr>( \
        Configs::e, StringConcat::createString(desc))

constexpr auto kConfigsMap =
    array_helpers::make<static_cast<int>(Configs::MAX), Configs, ConfigStr>(
        CONFIG_AND_STR(TOKEN), CONFIG_AND_STR(SRC_ROOT), CONFIG_AND_STR(PATH),
        CONFIG_AND_STR(LOG_FILE), CONFIG_AND_STR(DATABASE_BACKEND),
        CONFIG_AND_STR(HELP), CONFIG_AND_STR(OVERRIDE_CONF),
        CONFIG_AND_STR(SOCKET_BACKEND), CONFIG_AND_STR(SELECTOR),
        CONFIG_AND_STR(LOCALE));

constexpr auto kConfigsAliasMap =
    array_helpers::make<static_cast<int>(Configs::MAX), Configs, const char>(
        CONFIGALIAS_AND_STR(TOKEN, 't'), CONFIGALIAS_AND_STR(SRC_ROOT, 'r'),
        CONFIGALIAS_AND_STR(PATH, 'p'), CONFIGALIAS_AND_STR(LOG_FILE, 'f'),
        CONFIGALIAS_AND_STR(DATABASE_BACKEND, 'd'),
        CONFIGALIAS_AND_STR(HELP, 'h'), CONFIGALIAS_AND_STR(OVERRIDE_CONF, 'c'),
        CONFIGALIAS_AND_STR(SOCKET_BACKEND, 's'),
        CONFIGALIAS_AND_STR(SELECTOR, 'u'), CONFIGALIAS_AND_STR(LOCALE, 'l'));

constexpr auto kConfigsDescMap =
    array_helpers::make<static_cast<int>(Configs::MAX), Configs, DescStr>(
        DESC_AND_STR(TOKEN, "Bot Token"),
        DESC_AND_STR(SRC_ROOT, "Root directory of source tree"),
        DESC_AND_STR(PATH, "Environment variable PATH (to override)"),
        DESC_AND_STR(LOG_FILE, "File path to log"),
        DESC_AND_STR(DATABASE_BACKEND, "Database backend to use"),
        DESC_AND_STR(HELP, "Print this help message"),
        DESC_AND_STR(OVERRIDE_CONF, "Override config file"),
        DESC_AND_STR(SOCKET_BACKEND, "Socket backend to use"),
        DESC_AND_STR(SELECTOR, "Selector(poll(2), etc...) backend to use"),
        DESC_AND_STR(LOCALE, "Locale of the language to use (Current: en,fr)"));

/**
 * getVariable - Function used to retrieve the value of a specific
 * configuration.
 *
 * @param config The configuration for which the value is to be retrieved.
 * @return A std::optional containing the value of the specified configuration,
 * or std::nullopt if the configuration is not found.
 */
TgBotUtils_API std::optional<std::string> getVariable(Configs config);

/**
 * setVariable - Function used to set the value of a specific configuration.
 *
 * @param config The configuration for which the value is to be set.
 * @param value The new value to be set for the specified configuration.
 */
TgBotUtils_API void setVariable(Configs config, const std::string &value);

/**
 * getEnv - Function used to retrieve the value of an environment variable.
 *
 * @param name The name of the environment variable for which the value is to be
 * retrieved.
 * @param value [out] A reference to a string where the retrieved value will be
 * stored.
 * @return A boolean indicating whether the environment variable was found and
 * its value was successfully retrieved. If the environment variable is not
 * found, the function returns false and the value of 'value' remains unchanged.
 *
 * @note This function uses the standard library function std::getenv to
 * retrieve the value of the environment variable. If the environment variable
 * is not found, std::getenv returns a null pointer, which is converted to false
 * in this function.
 *
 * @note The retrieved value is stored in the 'value' parameter as a string. If
 * the environment variable is found but its value is empty, the 'value'
 * parameter will be set to an empty string.
 */
TgBotUtils_API bool getEnv(const std::string &name, std::string &value);

/**
 * serializeHelpToOStream - Function used to serialize the help information to
 * an output stream.
 *
 * @param out The output stream to which the help information will be
 * serialized.
 */
TgBotUtils_API void serializeHelpToOStream(std::ostream &out);

};  // namespace ConfigManager

