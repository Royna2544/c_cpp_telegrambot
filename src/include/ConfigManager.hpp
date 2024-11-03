#pragma once

#include <TgBotUtilsExports.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "CompileTimeStringConcat.hpp"
#include "EnumArrayHelpers.h"
#include "trivial_helpers/fruit_inject.hpp"

// Abstract manager for config loader
// Currently have three sources, env and file, cmdline
class TgBotUtils_API ConfigManager {
   public:
    enum class Configs {
        TOKEN,
        LOG_FILE,
        DATABASE_CFG,
        HELP,
        OVERRIDE_CONF,
        SOCKET_CFG,
        SELECTOR_CFG,
        MAX
    };

    /**
     * get - Function used to retrieve the value of a specific
     * configuration.
     *
     * @param config The configuration for which the value is to be retrieved.
     * @return A std::optional containing the value of the specified
     * configuration, or std::nullopt if the configuration is not found.
     */
    std::optional<std::string> get(Configs config);

    /**
     * set - Function used to set the value of a specific configuration.
     *
     * @param config The configuration for which the value is to be set.
     * @param value The new value to be set for the specified configuration.
     */
    static void set(Configs config, const std::string& value);

    /**
     * getEnv - Function used to retrieve the value of an environment variable.
     *
     * @param name The name of the environment variable for which the value is
     * to be retrieved.
     * @param value [out] A reference to a string where the retrieved value will
     * be stored.
     * @return A boolean indicating whether the environment variable was found
     * and its value was successfully retrieved. If the environment variable is
     * not found, the function returns false and the value of 'value' remains
     * unchanged.
     *
     * @note This function uses the standard library function std::getenv to
     * retrieve the value of the environment variable. If the environment
     * variable is not found, std::getenv returns a null pointer, which is
     * converted to false in this function.
     *
     * @note The retrieved value is stored in the 'value' parameter as a string.
     * If the environment variable is found but its value is empty, the 'value'
     * parameter will be set to an empty string.
     */
    static bool getEnv(const std::string& name, std::string& value);

    /**
     * serializeHelpToOStream - Function used to serialize the help information
     * to an output stream.
     *
     * @param out The output stream to which the help information will be
     * serialized.
     */
    static void serializeHelpToOStream(std::ostream& out);

    // Constructor
    APPLE_INJECT(ConfigManager(int argc, char* const* argv));

    [[nodiscard]] char* const* argv() const;
    [[nodiscard]] int argc() const;
    [[nodiscard]] std::filesystem::path exe() const;

   private:
    int _argc;
    char* const* _argv;
    std::filesystem::path startingDirectory;

    using ConfigStr = StringConcat::String<20>;
    using DescStr = StringConcat::String<50>;

#define CONFIG_AND_STR(e)                                        \
    array_helpers::make_elem<ConfigManager::Configs, ConfigStr>( \
        ConfigManager::Configs::e, StringConcat::createString(#e))

#define CONFIGALIAS_AND_STR(e, alias)                             \
    array_helpers::make_elem<ConfigManager::Configs, const char>( \
        ConfigManager::Configs::e, alias)

#define DESC_AND_STR(e, desc)                                  \
    array_helpers::make_elem<ConfigManager::Configs, DescStr>( \
        ConfigManager::Configs::e, StringConcat::createString(desc))

   public:
    constexpr static auto kConfigsMap =
        array_helpers::make<static_cast<int>(ConfigManager::Configs::MAX),
                            ConfigManager::Configs, ConfigStr>(
            CONFIG_AND_STR(TOKEN), CONFIG_AND_STR(LOG_FILE),
            CONFIG_AND_STR(DATABASE_CFG), CONFIG_AND_STR(HELP),
            CONFIG_AND_STR(OVERRIDE_CONF), CONFIG_AND_STR(SOCKET_CFG),
            CONFIG_AND_STR(SELECTOR_CFG));

    constexpr static auto kConfigsAliasMap =
        array_helpers::make<static_cast<int>(ConfigManager::Configs::MAX),
                            ConfigManager::Configs, const char>(
            CONFIGALIAS_AND_STR(TOKEN, 't'), CONFIGALIAS_AND_STR(LOG_FILE, 'f'),
            CONFIGALIAS_AND_STR(DATABASE_CFG, 'd'),
            CONFIGALIAS_AND_STR(HELP, 'h'),
            CONFIGALIAS_AND_STR(OVERRIDE_CONF, 'c'),
            CONFIGALIAS_AND_STR(SOCKET_CFG, 's'),
            CONFIGALIAS_AND_STR(SELECTOR_CFG, 'u'));

    constexpr static auto kConfigsDescMap =
        array_helpers::make<static_cast<int>(ConfigManager::Configs::MAX),
                            ConfigManager::Configs, DescStr>(
            DESC_AND_STR(TOKEN, "Bot Token"),
            DESC_AND_STR(LOG_FILE, "File path to log"),
            DESC_AND_STR(DATABASE_CFG, "Database backend to use"),
            DESC_AND_STR(HELP, "Print this help message"),
            DESC_AND_STR(OVERRIDE_CONF, "Override config file"),
            DESC_AND_STR(SOCKET_CFG, "Socket backend to use"),
            DESC_AND_STR(SELECTOR_CFG,
                         "Selector(poll(2), etc...) backend to use"));

    struct Backend {
        virtual ~Backend() = default;
        constexpr static std::string_view kConfigOverrideVar =
            array_helpers::find(ConfigManager::kConfigsMap,
                                ConfigManager::Configs::OVERRIDE_CONF)
                ->second;

        virtual bool load() { return true; }
        virtual std::optional<std::string> get(const std::string& name) = 0;
        virtual bool doOverride(const std::string& /*config*/) { return false; }

        /**
         * @brief This field stores the name of the backend.
         *
         * This field stores the name of the backend, such as "Command line" or
         * "File". This field is used for logging purposes.
         */
        [[nodiscard]] virtual std::string_view name() const = 0;
    };

   private:
    std::vector<std::unique_ptr<Backend>> backends;
};
