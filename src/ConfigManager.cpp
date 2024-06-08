#include <ConfigManager.h>
#include <absl/log/log.h>

#include <boost/program_options.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <libos/libfs.hpp>
#include <memory>
#include <mutex>
#include <optional>
#include <ostream>
#include <string_view>

#include "CompileTimeStringConcat.hpp"

namespace po = boost::program_options;
using namespace ConfigManager;
using namespace StringConcat;

template <typename T>
auto SingleQuoted(T t) {
    return std::quoted(t, '\'');
}

template <typename T, Configs config>
void AddOption(po::options_description &desc) {
    constexpr int index = static_cast<int>(config);
    constexpr auto confstr =
        cat(kConfigsMap.at(index).second, createString(','),
            createString(kConfigsAliasMap.at(index).second));

    desc.add_options()(confstr.c, po::value<T>(),
                       kConfigsDescMap.at(index).second.c);
}

template <Configs config>
void AddOption(po::options_description &desc) {
    constexpr int index = static_cast<int>(config);
    constexpr auto confstr =
        StringConcat::cat(kConfigsMap.at(index).second, createString(','),
                          createString(kConfigsAliasMap.at(index).second));

    desc.add_options()(confstr.c, kConfigsDescMap.at(index).second.c);
}

struct ConfigBackendBase {
    virtual ~ConfigBackendBase() = default;
    constexpr static std::string_view kConfigOverrideVar =
        array_helpers::find(ConfigManager::kConfigsMap, Configs::OVERRIDE_CONF)
            ->second;

    virtual bool load() { return true; }
    virtual std::optional<std::string> getVariable(const std::string &name) = 0;
    virtual bool doOverride(const std::string & /*config*/) { return false; }

    /**
     * @brief This field stores the name of the backend.
     *
     * This field stores the name of the backend, such as "Command line" or
     * "File". This field is used for logging purposes.
     */
    [[nodiscard]] virtual std::string_view getName() const = 0;
};

struct ConfigBackendEnv : public ConfigBackendBase {
    ~ConfigBackendEnv() override = default;
    ConfigBackendEnv() = default;

    std::optional<std::string> getVariable(const std::string &name) override {
        std::string outvalue;
        if (ConfigManager::getEnv(name, outvalue)) {
            return outvalue;
        }
        return std::nullopt;
    }

    bool doOverride(const std::string &config) override {
        std::string value;
        std::string kConfigOverrideVariable(kConfigOverrideVar);
        return getVariable(kConfigOverrideVariable) && value == config;
    }
    [[nodiscard]] std::string_view getName() const override { return "Env"; }
};

struct ConfigBackendBoostPOBase : public ConfigBackendBase {
    static po::options_description getTgBotOptionsDesc() {
        static po::options_description desc("TgBot++ Configs");
        static std::once_flag once;
        std::call_once(once, [] {
            AddOption<std::string, Configs::TOKEN>(desc);
            AddOption<std::string, Configs::SRC_ROOT>(desc);
            AddOption<std::string, Configs::PATH>(desc);
            AddOption<std::string, Configs::LOG_FILE>(desc);
            AddOption<std::string, Configs::DATABASE_BACKEND>(desc);
            AddOption<std::string, Configs::OVERRIDE_CONF>(desc);
        });
        return desc;
    }

    std::optional<std::string> getVariable(const std::string &name) override {
        if (name != kConfigOverrideVar) {
            if (const auto it = mp[name]; !it.empty()) {
                return it.as<std::string>();
            }
        }
        return std::nullopt;
    }

    bool doOverride(const std::string &config) override {
        if (const auto it = mp[kConfigOverrideVar.data()]; !it.empty()) {
            const auto &vec = it.as<std::vector<std::string>>();
            return std::ranges::find(vec, config) != vec.end();
        }

        return false;
    }

    ConfigBackendBoostPOBase() = default;
    virtual ~ConfigBackendBoostPOBase() = default;

   protected:
    po::variables_map mp;
};

struct ConfigBackendFile : public ConfigBackendBoostPOBase {
    [[deprecated]]
    static constexpr std::string_view kTgBotConfigFileOld = ".tgbot_conf.ini";
    static constexpr std::string_view kTgBotConfigFile = "tgbotserver.ini";
    bool load() override {
        std::filesystem::path home;
        std::string line;

        home = FS::getPathForType(FS::PathType::HOME);
        if (home.empty()) {
            return false;
        }
        auto confPath = (home / kTgBotConfigFile.data()).string();
        std::ifstream ifs(confPath);
        if (ifs.fail()) {
            LOG(ERROR) << "Opening " << confPath << " failed";

            confPath = (home / kTgBotConfigFileOld.data()).string();
            ifs.open(confPath);
            if (ifs.fail()) {
                LOG(ERROR) << "Opening " << confPath << " failed";
                return false;
            } else {
                LOG(WARNING) << "Using " << confPath << " instead, but"
                             << " this path is deprecated, and will be removed "
                                "in the future";
            }
        }
        try {
            po::store(po::parse_config_file(ifs, getTgBotOptionsDesc()), mp);
        } catch (const boost::program_options::error &e) {
            LOG(ERROR) << "File backend failed to parse: " << e.what();
            return false;
        }
        po::notify(mp);

        LOG(INFO) << "Loaded " << mp.size() << " entries from " << confPath;
        return true;
    }
    [[nodiscard]] std::string_view getName() const override { return "File"; }

    ConfigBackendFile() = default;
    ~ConfigBackendFile() override = default;
};

struct ConfigBackendCmdline : public ConfigBackendBoostPOBase {
    static po::options_description getTgBotOptionsDesc() {
        auto desc = ConfigBackendBoostPOBase::getTgBotOptionsDesc();

        AddOption<Configs::HELP>(desc);
        return desc;
    }

    bool load() override {
        int argc = 0;
        char *const *argv = nullptr;

        copyCommandLine(CommandLineOp::GET, &argc, &argv);
        if (argv == nullptr) {
            LOG(WARNING)
                << "Command line copy failed, probably it wasn't saved before";
            return false;
        }

        try {
            po::store(po::parse_command_line(argc, argv, getTgBotOptionsDesc()),
                      mp);
        } catch (const boost::program_options::error &e) {
            LOG(ERROR) << "Cmdline backend failed to parse: " << e.what();
            return false;
        }
        po::notify(mp);

        LOG(INFO) << "Loaded " << mp.size() << " entries (cmdline)";
        return true;
    }
    [[nodiscard]] std::string_view getName() const override {
        return "Cmdline";
    }

    ConfigBackendCmdline() = default;
    ~ConfigBackendCmdline() override = default;
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

namespace details {

std::vector<std::unique_ptr<ConfigBackendBase>> &getAvailableBackends() {
    static std::vector<std::unique_ptr<ConfigBackendBase>> backends;
    static std::once_flag status;
    std::call_once(status, [] {
        auto cmdline = std::make_unique<ConfigBackendCmdline>();
        if (cmdline->load()) {
            backends.emplace_back(std::move(cmdline));
        }
        auto file = std::make_unique<ConfigBackendFile>();
        if (file->load()) {
            backends.emplace_back(std::move(file));
        }
        auto env = std::make_unique<ConfigBackendEnv>();
        if (env->load()) {
            backends.emplace_back(std::move(env));
        }
        DLOG(INFO) << "Loaded " << backends.size() << " backends";
    });
    return backends;
}

}  // namespace details

std::optional<std::string> getVariable(Configs config) {
    Passes p = Passes::INIT;
    std::string outvalue;
    std::string name = array_helpers::find(kConfigsMap, config)->second;
    auto const &backends = details::getAvailableBackends();
    name.resize(strlen(name.c_str()));

    p = Passes::FIND_OVERRIDE;
    do {
        switch (p) {
            case Passes::FIND_OVERRIDE:
                for (const auto &bit : backends) {
                    if (bit->doOverride(name)) {
                        DLOG(INFO) << "Used " << SingleQuoted(bit->getName())
                                   << " backend for fetching var "
                                   << SingleQuoted(name) << " (forced)";
                        return bit->getVariable(name);
                    }
                }
                p = Passes::ACTUAL_GET;
                break;
            case Passes::ACTUAL_GET:
                for (const auto &bit : backends) {
                    const auto &result = bit->getVariable(name);
                    if (result.has_value()) {
                        DLOG(INFO) << "Used " << SingleQuoted(bit->getName())
                                   << " backend for fetching var "
                                   << SingleQuoted(name);
                        return result;
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
    out << ConfigBackendCmdline::getTgBotOptionsDesc() << std::endl;
}

}  // namespace ConfigManager
