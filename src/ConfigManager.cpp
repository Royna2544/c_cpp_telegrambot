#include <ConfigManager.h>
#include <Logging.h>

#include <boost/program_options.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <libos/libfs.hpp>
#include <memory>
#include <mutex>
#include <optional>

/**
 * @brief This struct represents a configuration backend.
 *
 * A configuration backend is a source of configuration variables, such as the
 * command line, a configuration file, or environment variables.
 */
struct ConfigBackendBase {
    /**
     * @brief This function loads the configuration data from the backend.
     *
     * This function should load the configuration data from the backend and
     * store it in the priv field. The function should return a pointer to the
     * loaded data, or nullptr if the load failed.
     *
     * The data should be stored in a void* pointer, and the initialized field
     * should be set to false to indicate that the data is not loaded.
     *
     * This function should only be called once, and the data should be valid
     * for the lifetime of the program.
     */
    virtual void *load() { return nullptr; }

    /**
     * @brief This function gets a configuration variable from the backend.
     *
     * This function should retrieve the value of a configuration variable from
     * the backend. The function takes a pointer to the private data for the
     * backend, a variable name, and an output value for the retrieved variable.
     *
     * The function should return true if the variable was found and retrieved,
     * or false if the variable was not found or could not be retrieved.
     *
     * If the variable was found, the output value should be set to the variable
     * value and the function should return true. If the variable was not found,
     * the output value should be left unchanged and the function should return
     * false.
     */
    virtual bool getVariable(const void *priv, const std::string &name,
                             std::string &outvalue) = 0;

    /**
     * @brief This function determines if the backend should be used for a
     * configuration variable.
     *
     * This function should determine if the backend should be used for a
     * configuration variable. The function takes a pointer to the private data
     * for the backend, and a configuration variable name.
     *
     * The function should return true if the backend should be used for the
     * variable, or false if the backend should not be used.
     *
     * If the function returns true, the backend's getVariable() function will
     * be called to retrieve the variable value. If the function returns false,
     * the next backend in the list will be checked.
     */
    virtual bool doOverride(const void *priv, const std::string &config) {
        return false;
    }

    /**
     * @brief This field stores the name of the backend.
     *
     * This field stores the name of the backend, such as "Command line" or
     * "File". This field is used for logging purposes.
     */
    virtual const char *getName() const = 0;

    /**
     * @brief This field stores the private data for the backend.
     *
     * This field stores the private data for the backend, which is specific to
     * the backend implementation. The data should be loaded by the load()
     * function and used by the other functions.
     */
    struct {
        void *data = nullptr;
        bool initialized = false;
    } priv;
};

struct ConfigBackendEnv : public ConfigBackendBase {
    bool getVariable(const void *priv, const std::string &name,
                     std::string &outvalue) override {
        char *buf = getenv(name.c_str());
        if (buf) {
            outvalue = buf;
            return true;
        }
        return false;
    }
    const char *getName() const override { return "Env"; }
};

namespace po = boost::program_options;
struct ConfigBackendFile : public ConfigBackendBase {
    struct file_priv {
        constexpr static const char kConfigOverrideVar[] = "OVERRIDE_CONF";
        static po::options_description getTgBotOptionsDesc() {
            static po::options_description desc("TgBot++ Configs");
            static std::once_flag once;
            std::call_once(once, [] {
                desc.add_options()
                    // clang-format off
                ("TOKEN,t", po::value<std::string>(), "Bot Token")
                ("SRC_ROOT,r", po::value<std::string>(), "Root directory of source tree")
                ("PATH,p", po::value<std::string>(), "Environment variable PATH (to override)")
                (file_priv::kConfigOverrideVar, po::value<std::vector<std::string>>()->multitoken(),
                    "Config list to override from this source");
                // clang-format on
            });
            return desc;
        }
        po::variables_map mp;
    };

    bool getVariable(const void *p, const std::string &name,
                     std::string &outvalue) override {
        auto priv = reinterpret_cast<const file_priv *>(p);
        if (priv && name != file_priv::kConfigOverrideVar) {
            if (const auto it = priv->mp[name]; !it.empty()) {
                outvalue = it.as<std::string>();
                return true;
            }
        }
        return false;
    }

    bool doOverride(const void *p, const std::string &config) override {
        auto priv = reinterpret_cast<const file_priv *>(p);
        if (priv) {
            if (const auto it = priv->mp[file_priv::kConfigOverrideVar];
                !it.empty()) {
                const auto vec = it.as<std::vector<std::string>>();
                return std::find(vec.begin(), vec.end(), config) != vec.end();
            }
        }
        return false;
    }

    virtual void *load() override {
        std::filesystem::path home;
        std::string line;
        static file_priv p{};

        home = FS::getPathForType(FS::PathType::HOME);
        if (home.empty()) {
            return nullptr;
        }
        const auto confPath = (home / ".tgbot_conf.ini").string();
        std::ifstream ifs(confPath);
        if (ifs.fail()) {
            LOG(LogLevel::ERROR, "Opening %s failed", confPath.c_str());
            return nullptr;
        }
        po::store(po::parse_config_file(ifs, file_priv::getTgBotOptionsDesc()),
                  p.mp);
        po::notify(p.mp);

        LOG(LogLevel::INFO, "Loaded %zu entries from %s", p.mp.size(),
            confPath.c_str());
        return &p;
    }
    virtual const char *getName() const override { return "File"; }
};

struct ConfigBackendCmdline : public ConfigBackendFile {
    struct cmdline_priv : ConfigBackendFile::file_priv {
        static po::options_description getTgBotOptionsDesc() {
            auto desc = file_priv::getTgBotOptionsDesc();
            desc.add_options()("help,h", "Help message for this bot vars");
            return desc;
        }
    };
    void *load() override {
        static cmdline_priv p{};
        int argc = 0;
        char *const *argv = nullptr;

        copyCommandLine(CommandLineOp::GET, &argc, &argv);
        if (!argv) {
            LOG(LogLevel::WARNING,
                "Command line copy failed, probably it wasn't saved before");
            return nullptr;
        }

        po::store(po::parse_command_line(argc, argv,
                                         cmdline_priv::getTgBotOptionsDesc()),
                  p.mp);
        po::notify(p.mp);

        LOG(LogLevel::INFO, "Loaded %zu entries", p.mp.size());
        return &p;
    }
    const char *getName() const override { return "Cmdline"; }
};

void copyCommandLine(CommandLineOp op, int *argc, char *const **argv) {
    static int argc_internal = 0;
    static char *const *argv_internal = nullptr;

    switch (op) {
        case INSERT:
            argc_internal = *argc;
            argv_internal = *argv;
            break;
        case GET:
            *argv = argv_internal;
            *argc = argc_internal;
            break;
    };
}

namespace ConfigManager {

enum class Passes {
    INIT,
    FIND_OVERRIDE,
    ACTUAL_GET,
    DONE,
};

std::optional<std::string> getVariable(const std::string &name) {
    static std::vector<std::shared_ptr<ConfigBackendBase>> backends = {
        std::make_shared<ConfigBackendCmdline>(),
        std::make_shared<ConfigBackendEnv>(),
        std::make_shared<ConfigBackendFile>(),
    };

    Passes p = Passes::INIT;
    std::string outvalue;

    for (auto &bit : backends) {
        if (!bit->priv.initialized) {
            bit->priv.data = bit->load();
            bit->priv.initialized = true;
        }
    }

    p = Passes::FIND_OVERRIDE;
    do {
        switch (p) {
            case Passes::FIND_OVERRIDE:
                for (auto &bit : backends) {
                    if (bit->doOverride(bit->priv.data, name)) {
                        LOG(LogLevel::VERBOSE,
                            "Used '%s' backend for fetching var '%s' "
                            "(forced)",
                            bit->getName(), name.c_str());
                        bit->getVariable(bit->priv.data, name, outvalue);
                        return {outvalue};
                    }
                }
                p = Passes::ACTUAL_GET;
                break;
            case Passes::ACTUAL_GET:
                for (auto &bit : backends) {
                    if (bit->getVariable(bit->priv.data, name, outvalue)) {
                        LOG(LogLevel::VERBOSE,
                            "Used '%s' backend for fetching var '%s'",
                            bit->getName(), name.c_str());
                        return {outvalue};
                    }
                }
                p = Passes::DONE;
                break;
            default:
                ASSERT_UNREACHABLE;
                break;
        }

    } while (p != Passes::DONE);

    return std::nullopt;
}

void printHelp() {
    std::cout << ConfigBackendCmdline::cmdline_priv::getTgBotOptionsDesc()
              << std::endl;
}

}  // namespace ConfigManager
