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
extern const char* const sectionFilePath;

// Abstract manager for config loader
// Currently have three sources, env and file, cmdline
class UTILS_EXPORT ConfigManager {
   public:
    enum class Configs {
        TOKEN,
        LOG_FILE,
        DATABASE_CFG,
        HELP,
        SOCKET_CFG,
        GITHUB_TOKEN,
        OPTIONAL_COMPONENTS,
        BUILDBUDDY_API_KEY,
        LLMCONFIG,
        FILEPATH_ROM_BUILD,
        FILEPATH_KERNEL_BUILD,
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
            .config = Configs::DATABASE_CFG,
            .name = "DatabaseCfg",
            .description = "Database configuration",
            .alias = 'd',
            .type = Entry::ArgType::STRING,
            .belongsTo = &sectionMain,
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
            .config = Configs::SOCKET_CFG,
            .name = "SocketCfg",
            .description = "Sockets (ipv4/ipv6)",
            .alias = Entry::ALIAS_NONE,
            .type = Entry::ArgType::STRING,
            .belongsTo = &sectionMain,
        },
        {
            .config = Configs::GITHUB_TOKEN,
            .name = "GithubToken",
            .description = "Github token",
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
            .config = Configs::LLMCONFIG,
            .name = "LLMConfig",
            /*
             * LLM configuration format:
             * 1. local,filepath - for local LLM models
             * Example: local,/path/to/model.gguf. Supported models are the ones
             * that llama.cpp supports.
             * 2. localnet,urlendpoint - for LLM models served over network
             * Example: localnet,http://localhost:8000/api/v1/
             * Include authkey if needed:
             * It will be sent as Bearer token in Authorization header.
             * localnet,http://localhost:8000/api/v1/,mysecretkey
             */
            .description = "LLM configuration. "
                           "(local/localnet),(filepath/urlendpoint)(,authkey)",
            .alias = Entry::ALIAS_NONE,
            .type = Entry::ArgType::STRING,
            .belongsTo = &sectionMain,
        },
        {
            .config = Configs::FILEPATH_ROM_BUILD,
            .name = "ROMBuild",
            .description = "Directory to the ROM build",
            .alias = Entry::ALIAS_NONE,
            .type = Entry::ArgType::STRING,
            .belongsTo = &sectionFilePath,
        },
        {
            .config = Configs::FILEPATH_KERNEL_BUILD,
            .name = "KernelBuild",
            .description = "Directory to the Kernel build",
            .alias = Entry::ALIAS_NONE,
            .type = Entry::ArgType::STRING,
            .belongsTo = &sectionFilePath,
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
