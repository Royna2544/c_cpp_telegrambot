#pragma once

#include <UtilsExports.h>

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "CommandLine.hpp"
#include "trivial_helpers/fruit_inject.hpp"

// Abstract manager for config loader
// Currently have three sources, env and file, cmdline
class Utils_API ConfigManager {
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
        VIRUSTOTAL_API_KEY,
        MAX
    };
    static constexpr size_t CONFIG_MAX = static_cast<int>(Configs::MAX);

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
        enum class ArgType { NONE, STRING } type;
    };

    static constexpr std::array<Entry, CONFIG_MAX> kConfigMap = {
        Entry{
            Configs::TOKEN,
            "TOKEN",
            "Telegram bot token",
            't',
            Entry::ArgType::STRING,
        },
        {
            Configs::LOG_FILE,
            "LOG_FILE",
            "Log file path",
            'f',
            Entry::ArgType::STRING,
        },
        {
            Configs::DATABASE_CFG,
            "DATABASE_CFG",
            "Database configuration",
            'd',
            Entry::ArgType::STRING,
        },
        {
            Configs::HELP,
            "HELP",
            "Display help information",
            'h',
            Entry::ArgType::NONE,
        },
        {
            Configs::SOCKET_CFG,
            "SOCKET_CFG",
            "Sockets (ipv4/ipv6/local)",
            Entry::ALIAS_NONE,
            Entry::ArgType::STRING,
        },
        {
            Configs::SELECTOR_CFG,
            "SELECTOR_CFG",
            "Selectors (poll/epoll/select)",
            Entry::ALIAS_NONE,
            Entry::ArgType::STRING,
        },
        {
            Configs::GITHUB_TOKEN,
            "GITHUB_TOKEN",
            "Github token",
            Entry::ALIAS_NONE,
            Entry::ArgType::STRING,
        },
        {
            Configs::OPTIONAL_COMPONENTS,
            "OPTIONAL_COMPONENTS",
            "Enable optional components (webserver/datacollector)",
            Entry::ALIAS_NONE,
            Entry::ArgType::STRING,
        },
        {
            Configs::VIRUSTOTAL_API_KEY,
            "VIRUSTOTAL_API_KEY",
            "VirusTotal API key",
            Entry::ALIAS_NONE,
            Entry::ArgType::STRING,
        },
    };

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
    enum class BackendType { COMMAND_LINE, ENV, FILE, MAX };

    class BackendStorage {
        std::array<std::unique_ptr<Backend>, static_cast<int>(BackendType::MAX)>
            backends;

       public:
        std::unique_ptr<Backend>& operator[](const BackendType type) {
            return backends[static_cast<int>(type)];
        }

        [[nodiscard]] decltype(backends)::const_iterator begin() const {
            return backends.cbegin();
        }

        [[nodiscard]] decltype(backends)::const_iterator end() const {
            return backends.cend();
        }

        [[nodiscard]] size_t size() const {
            return std::ranges::count_if(
                backends, [](const auto& ent) { return ent != nullptr; });
        }
    } storage;
    // CommandLine, Env, File
};
