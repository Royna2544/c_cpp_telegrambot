#pragma once

#include <TgBotUtilsExports.h>

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "CommandLine.hpp"
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
        SOCKET_CFG,
        SELECTOR_CFG,
        GITHUB_TOKEN,
        OPTIONAL_COMPONENTS,
        MAX
    };
    static constexpr int CONFIG_MAX = static_cast<int>(Configs::MAX);

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
    static bool getEnv(const std::string_view name, std::string& value);

    /**
     * serializeHelpToOStream - Function used to serialize the help information
     * to an output stream.
     *
     * @param out The output stream to which the help information will be
     * serialized.
     */
    static void serializeHelpToOStream(std::ostream& out);

    // Constructor
    APPLE_EXPLICIT_INJECT(ConfigManager(CommandLine line));

    struct Entry {
        static constexpr char ALIAS_NONE = '\0';

        Configs config;
        std::string_view name;
        std::string_view description;
        char alias;
    };

    // clang-format off
    static constexpr std::array<Entry, CONFIG_MAX> kConfigMap = {
        Entry{Configs::TOKEN, "TOKEN", "Telegram bot token", 't'},
        {Configs::LOG_FILE, "LOG_FILE", "Log file path", 'f'},
        {Configs::DATABASE_CFG, "DATABASE_CFG", "Database configuration file path", 'd'},
        {Configs::HELP, "HELP", "Display help information", 'h'},
        {Configs::SOCKET_CFG, "SOCKET_CFG", "Sockets (ipv4/ipv6/local)", Entry::ALIAS_NONE},
        {Configs::SELECTOR_CFG, "SELECTOR_CFG", "Selectors (poll/epoll/select)", Entry::ALIAS_NONE},
        {Configs::GITHUB_TOKEN, "GITHUB_TOKEN", "Github token", Entry::ALIAS_NONE},
        {Configs::OPTIONAL_COMPONENTS, "OPTIONAL_COMPONENTS", "Enable optional components (webserver/datacollector)", Entry::ALIAS_NONE}
    };
    // clang-format on

    struct Backend {
        virtual ~Backend() = default;

        virtual bool load() { return true; }
        virtual std::optional<std::string> get(const std::string_view name) = 0;

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
