#include <ConfigManager.h>
#include <absl/log/log.h>

#include <boost/program_options.hpp>
#include <boost/program_options/errors.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/value_semantic.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <libos/libfs.hpp>
#include <memory>
#include <mutex>
#include <optional>
#include <ostream>
#include <string_view>
#include <tuple>

#include "CompileTimeStringConcat.hpp"

namespace po = boost::program_options;
using namespace ConfigManager;

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
    virtual const std::string_view getName() const = 0;

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
    const std::string_view getName() const override { return "Env"; }
};

template <typename T, Configs config>
void AddOption(po::options_description &desc) {
    constexpr int index = static_cast<int>(config);
    constexpr auto confstr = StringConcat::cat(
        kConfigsMap.at(index).second, StringConcat::String<2>(','),
        StringConcat::String<2>(kConfigsAliasMap.at(index).second));

    desc.add_options()(confstr.c, po::value<T>(),
                       kConfigsDescMap.at(index).second.c);
}

template <Configs config>
void AddOption(po::options_description &desc) {
    constexpr int index = static_cast<int>(config);
    constexpr auto confstr = StringConcat::cat(
        kConfigsMap.at(index).second, StringConcat::String<2>(','),
        StringConcat::String<2>(kConfigsAliasMap.at(index).second));

    desc.add_options()(confstr.c, kConfigsDescMap.at(index).second.c);
}

struct ConfigBackendBoostPOBase : public ConfigBackendBase {
    struct boost_priv {
        constexpr static std::string_view kConfigOverrideVar = "OVERRIDE_CONF";
        static po::options_description getTgBotOptionsDesc() {
            static po::options_description desc("TgBot++ Configs");
            static std::once_flag once;
            std::call_once(once, [] {
                AddOption<std::string, Configs::TOKEN>(desc);
                AddOption<std::string, Configs::SRC_ROOT>(desc);
                AddOption<std::string, Configs::PATH>(desc);
                AddOption<std::string, Configs::LOG_FILE>(desc);
                AddOption<std::string, Configs::DATABASE_BACKEND>(desc);
                desc.add_options()(
                    kConfigOverrideVar.data(),
                    po::value<std::vector<std::string>>()->multitoken(),
                    "Config list to override from this source");
            });
            return desc;
        }
        po::variables_map mp;
    };

    bool getVariable(const void *p, const std::string &name,
                     std::string &outvalue) override {
        const auto *priv = reinterpret_cast<const boost_priv *>(p);
        if ((priv != nullptr) && name != boost_priv::kConfigOverrideVar) {
            if (const auto it = priv->mp[name]; !it.empty()) {
                outvalue = it.as<std::string>();
                return true;
            }
        }
        return false;
    }

    bool doOverride(const void *p, const std::string &config) override {
        const auto *priv = reinterpret_cast<const boost_priv *>(p);
        if (priv != nullptr) {
            if (const auto it = priv->mp[boost_priv::kConfigOverrideVar.data()];
                !it.empty()) {
                const auto vec = it.as<std::vector<std::string>>();
                return std::find(vec.begin(), vec.end(), config) != vec.end();
            }
        }
        return false;
    }
    virtual ~ConfigBackendBoostPOBase() = default;
};

struct ConfigBackendFile : public ConfigBackendBoostPOBase {
    void *load() override {
        std::filesystem::path home;
        std::string line;

        home = FS::getPathForType(FS::PathType::HOME);
        if (home.empty()) {
            return nullptr;
        }
        const auto confPath = (home / ".tgbot_conf.ini").string();
        std::ifstream ifs(confPath);
        if (ifs.fail()) {
            LOG(ERROR) << "Opening " << confPath << " failed";
            return nullptr;
        }
        try {
            po::store(
                po::parse_config_file(ifs, boost_priv::getTgBotOptionsDesc()),
                p.mp);
        } catch (const boost::program_options::error &e) {
            LOG(ERROR) << "File backend failed to parse: " << e.what();
            return nullptr;
        }
        po::notify(p.mp);

        LOG(INFO) << "Loaded " << p.mp.size() << " entries from " << confPath;
        return &p;
    }
    const std::string_view getName() const override { return "File"; }
    ~ConfigBackendFile() override = default;

   private:
    boost_priv p{};
};

struct ConfigBackendCmdline : public ConfigBackendBoostPOBase {
    struct cmdline_priv : ConfigBackendBoostPOBase::boost_priv {
        static po::options_description getTgBotOptionsDesc() {
            auto desc = boost_priv::getTgBotOptionsDesc();

            AddOption<Configs::HELP>(desc);
            return desc;
        }
    };

    void *load() override {
        int argc = 0;
        char *const *argv = nullptr;

        copyCommandLine(CommandLineOp::GET, &argc, &argv);
        if (argv == nullptr) {
            LOG(WARNING)
                << "Command line copy failed, probably it wasn't saved before";
            return nullptr;
        }

        try {
            po::store(po::parse_command_line(argc, argv,
                                             boost_priv::getTgBotOptionsDesc()),
                      p.mp);
        } catch (const boost::program_options::error &e) {
            LOG(ERROR) << "Cmdline backend failed to parse: " << e.what();
            return nullptr;
        }
        po::notify(p.mp);

        LOG(INFO) << "Loaded " << p.mp.size() << " entries (cmdline)";
        return &p;
    }
    const std::string_view getName() const override { return "Cmdline"; }
    ~ConfigBackendCmdline() override = default;

   private:
    cmdline_priv p{};
};

void copyCommandLine(CommandLineOp op, int *argc, char *const **argv) {
    static int argc_internal = 0;
    static char *const *argv_internal = nullptr;

    switch (op) {
        case CommandLineOp::INSERT:
            argc_internal = *argc;
            argv_internal = *argv;
            break;
        case CommandLineOp::GET:
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

std::optional<std::string> getVariable(Configs config) {
    static std::vector<std::shared_ptr<ConfigBackendBase>> backends = {
        std::make_shared<ConfigBackendCmdline>(),
        std::make_shared<ConfigBackendEnv>(),
        std::make_shared<ConfigBackendFile>(),
    };
    Passes p = Passes::INIT;
    std::string outvalue;
    std::string name = array_helpers::find(kConfigsMap, config)->second;
    name.resize(strlen(name.c_str()));
    
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
                        DLOG(INFO) << "Used '" << bit->getName()
                                   << "' backend for fetching var '" << name
                                   << "' (forced)";
                        bit->getVariable(bit->priv.data, name, outvalue);
                        return {outvalue};
                    }
                }
                p = Passes::ACTUAL_GET;
                break;
            case Passes::ACTUAL_GET:
                for (auto &bit : backends) {
                    if (bit->getVariable(bit->priv.data, name, outvalue)) {
                        DLOG(INFO)
                            << "Used '" << bit->getName()
                            << "' backend for fetching var '" << name << "'";
                        return {outvalue};
                    }
                }
                p = Passes::DONE;
                break;
            default:
                LOG(FATAL) << "Should never reach here";
                break;
        }

    } while (p != Passes::DONE);

    return std::nullopt;
}

void serializeHelpToOStream(std::ostream &out) {
    out << ConfigBackendCmdline::cmdline_priv::getTgBotOptionsDesc()
        << std::endl;
}

}  // namespace ConfigManager
