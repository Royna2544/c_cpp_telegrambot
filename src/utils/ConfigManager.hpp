#pragma once

#include <UtilsExports.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "CommandLine.hpp"
#include "trivial_helpers/fruit_inject.hpp"

// Dummy externs to distinguish sections in config files
extern const char* const sectionMain;
extern const char* const sectionNetwork;
extern const char* const sectionDatabase;
extern const char* const sectionLLM;

// Abstract manager for config loader
// Currently have three sources, env and file, cmdline
class UTILS_EXPORT ConfigManager {
   public:
    enum class Configs {
        TOKEN,
        LOG_FILE,
        DATABASE_FILEPATH,
        DATABASE_TYPE,
        HELP,
        SOCKET_URL_PRIMARY,
        SOCKET_URL_SECONDARY,
        SOCKET_URL_LOGGING,
        GITHUB_TOKEN,
        OPTIONAL_COMPONENTS,
        BUILDBUDDY_API_KEY,
        LLM_TYPE,
        LLM_LOCATION,
        LLM_AUTHKEY,
        TELEGRAM_API_SERVER,
        KERNELBUILD_SERVER,
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
    std::optional<std::string> get(Configs config) const;

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
        enum class ArgType : std::uint8_t { NONE, STRING } type;
        const char* const* belongsTo = nullptr;
    };

    static constexpr std::array<Entry, CONFIG_MAX> kConfigMap = {
        Entry{
            .config = Configs::TOKEN,
            .name = "Token",
            .description = "Telegram bot token",
            .alias = 't',
            .type = Entry::ArgType::STRING,
            .belongsTo = &sectionMain,
        },
        {
            .config = Configs::LOG_FILE,
            .name = "LogFile",
            .description = "Log file path",
            .alias = 'f',
            .type = Entry::ArgType::STRING,
            .belongsTo = &sectionMain,
        },
        {
            .config = Configs::DATABASE_FILEPATH,
            .name = "FilePath",
            .description = "Database file path",
            .alias = 'd',
            .type = Entry::ArgType::STRING,
            .belongsTo = &sectionDatabase,
        },
        {
            .config = Configs::DATABASE_TYPE,
            .name = "Type",
            .description = "Database type",
            .alias = Entry::ALIAS_NONE,
            .type = Entry::ArgType::STRING,
            .belongsTo = &sectionDatabase,
        },
        {
            .config = Configs::HELP,
            .name = "Help",
            .description = "Display help information",
            .alias = 'h',
            .type = Entry::ArgType::NONE,
            .belongsTo = nullptr,
        },
        {
            .config = Configs::SOCKET_URL_PRIMARY,
            .name = "PrimarySocketUrl",
            .description = "Primary socket URL",
            .alias = Entry::ALIAS_NONE,
            .type = Entry::ArgType::STRING,
            .belongsTo = &sectionNetwork,
        },
        {
            .config = Configs::SOCKET_URL_SECONDARY,
            .name = "SecondarySocketUrl",
            .description = "Secondary socket URL",
            .alias = Entry::ALIAS_NONE,
            .type = Entry::ArgType::STRING,
            .belongsTo = &sectionNetwork,
        },
        {
            .config = Configs::SOCKET_URL_LOGGING,
            .name = "LoggingSocketUrl",
            .description = "Logging socket URL",
            .alias = Entry::ALIAS_NONE,
            .type = Entry::ArgType::STRING,
            .belongsTo = &sectionNetwork,
        },
        {
            .config = Configs::GITHUB_TOKEN,
            .name = "GitHubToken",
            .description = "GitHub token",
            .alias = Entry::ALIAS_NONE,
            .type = Entry::ArgType::STRING,
            .belongsTo = &sectionMain,
        },
        {
            .config = Configs::OPTIONAL_COMPONENTS,
            .name = "OptionalComponents",
            .description =
                "Enable optional components (webserver/datacollector)",
            .alias = Entry::ALIAS_NONE,
            .type = Entry::ArgType::STRING,
            .belongsTo = &sectionMain,
        },
        {
            .config = Configs::BUILDBUDDY_API_KEY,
            .name = "BuildBuddyApiKey",
            .description = "BuildBuddy API key",
            .alias = Entry::ALIAS_NONE,
            .type = Entry::ArgType::STRING,
            .belongsTo = &sectionMain,
        },
        {
            .config = Configs::LLM_TYPE,
            .name = "BackendType",
            .description = "LLM backend type (local, localnet)",
            .alias = Entry::ALIAS_NONE,
            .type = Entry::ArgType::STRING,
            .belongsTo = &sectionLLM,
        },
        {
            .config = Configs::LLM_LOCATION,
            .name = "ModelLocation",
            .description = "LLM location (file path or network address)",
            .alias = Entry::ALIAS_NONE,
            .type = Entry::ArgType::STRING,
            .belongsTo = &sectionLLM,
        },
        {
            .config = Configs::LLM_AUTHKEY,
            .name = "AuthKey",
            .description = "LLM authentication key (if required)",
            .alias = Entry::ALIAS_NONE,
            .type = Entry::ArgType::STRING,
            .belongsTo = &sectionLLM,
        },
        {
            .config = Configs::TELEGRAM_API_SERVER,
            .name = "ApiServer",
            .description = "Custom Telegram API server URL",
            .alias = Entry::ALIAS_NONE,
            .type = Entry::ArgType::STRING,
            .belongsTo = &sectionNetwork,
        },
        {
            .config = Configs::KERNELBUILD_SERVER,
            .name = "BuilderRSServer",
            .description = "Builder-RS gRPC server address",
            .alias = Entry::ALIAS_NONE,
            .type = Entry::ArgType::STRING,
            .belongsTo = &sectionNetwork,
        }};

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
