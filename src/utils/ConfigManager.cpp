#include <absl/log/log.h>
#include <fmt/format.h>

#include <ConfigManager.hpp>
#include <StringToolsExt.hpp>
#include <boost/program_options.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <libfs.hpp>
#include <memory>
#include <mutex>
#include <optional>
#include <ostream>
#include <string_view>

#include "CompileTimeStringConcat.hpp"

namespace po = boost::program_options;
using namespace StringConcat;

template <typename T, ConfigManager::Configs config>
void AddOption(po::options_description &desc) {
    constexpr int index = static_cast<int>(config);
    constexpr auto confstr =
        cat(ConfigManager::kConfigsMap.at(index).second, createString(','),
            createString(ConfigManager::kConfigsAliasMap.at(index).second));

    desc.add_options()(
        confstr.get().data(), po::value<T>(),
        ConfigManager::kConfigsDescMap.at(index).second.get().data());
}

template <ConfigManager::Configs config>
void AddOption(po::options_description &desc) {
    constexpr int index = static_cast<int>(config);
    constexpr auto confstr = StringConcat::cat(
        ConfigManager::kConfigsMap.at(index).second, createString(','),
        createString(ConfigManager::kConfigsAliasMap.at(index).second));

    desc.add_options()(
        confstr.get().data(),
        ConfigManager::kConfigsDescMap.at(index).second.get().data());
}

struct ConfigBackendEnv : public ConfigManager::Backend {
    ~ConfigBackendEnv() override = default;
    ConfigBackendEnv() = default;

    std::optional<std::string> get(const std::string &name) override {
        std::string outvalue;
        if (ConfigManager::getEnv(name, outvalue)) {
            return outvalue;
        }
        return std::nullopt;
    }

    bool doOverride(const std::string &config) override {
        std::string value;
        std::string kConfigOverrideVariable(kConfigOverrideVar);
        return get(kConfigOverrideVariable) && value == config;
    }
    [[nodiscard]] std::string_view name() const override { return "Env"; }
};

struct ConfigBackendBoostPOBase : public ConfigManager::Backend {
    static po::options_description getTgBotOptionsDesc() {
        static po::options_description desc("TgBot++ Configs");
        static std::once_flag once;
        std::call_once(once, [] {
            AddOption<std::string, ConfigManager::Configs::TOKEN>(desc);
            AddOption<std::string, ConfigManager::Configs::LOG_FILE>(desc);
            AddOption<std::string, ConfigManager::Configs::DATABASE_CFG>(desc);
            AddOption<std::string, ConfigManager::Configs::OVERRIDE_CONF>(desc);
            AddOption<std::string, ConfigManager::Configs::SOCKET_CFG>(desc);
            AddOption<std::string, ConfigManager::Configs::SELECTOR_CFG>(desc);
        });
        return desc;
    }

    std::optional<std::string> get(const std::string &name) override {
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
    ~ConfigBackendBoostPOBase() override = default;

   protected:
    po::variables_map mp;
};

struct ConfigBackendFile : public ConfigBackendBoostPOBase {
    static constexpr std::string_view kTgBotConfigFile = "tgbotserver.ini";
    bool load() override {
        std::filesystem::path home;
        std::string line;

        home = FS::getPath(FS::PathType::HOME);
        if (home.empty()) {
            return false;
        }
        auto confPath = (home / kTgBotConfigFile.data()).string();
        std::ifstream ifs(confPath);
        if (ifs.fail()) {
            LOG(ERROR) << "Opening " << confPath << " failed";
            return false;
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
    [[nodiscard]] std::string_view name() const override { return "File"; }

    ConfigBackendFile() = default;
    ~ConfigBackendFile() override = default;
};

struct ConfigBackendCmdline : public ConfigBackendBoostPOBase {
    char *const *_argv;
    int _argc;

    static po::options_description getTgBotOptionsDesc() {
        auto desc = ConfigBackendBoostPOBase::getTgBotOptionsDesc();

        AddOption<ConfigManager::Configs::HELP>(desc);
        return desc;
    }

    bool load() override {
        try {
            po::store(
                po::parse_command_line(_argc, _argv, getTgBotOptionsDesc()),
                mp);
        } catch (const boost::program_options::error &e) {
            LOG(ERROR) << "Cmdline backend failed to parse: " << e.what();
            return false;
        }
        po::notify(mp);

        LOG(INFO) << "Loaded " << mp.size() << " entries (cmdline)";
        return true;
    }
    [[nodiscard]] std::string_view name() const override { return "Cmdline"; }

    ConfigBackendCmdline(int argc, char *const *argv)
        : _argv(argv), _argc(argc) {}
    ~ConfigBackendCmdline() override = default;
};

enum class Passes {
    INIT,
    FIND_OVERRIDE,
    ACTUAL_GET,
    DONE,
};

ConfigManager::ConfigManager(int argc, char *const *argv)
    : _argc(argc),
      _argv(argv),
      startingDirectory(std::filesystem::current_path()) {
    auto cmdline = std::make_unique<ConfigBackendCmdline>(_argc, _argv);
    if (cmdline->load()) {
        backends.emplace_back(std::move(cmdline));
    }
    auto env = std::make_unique<ConfigBackendEnv>();
    if (env->load()) {
        backends.emplace_back(std::move(env));
    }
    auto file = std::make_unique<ConfigBackendFile>();
    if (file->load()) {
        backends.emplace_back(std::move(file));
    }
    DLOG(INFO) << "Loaded " << backends.size() << " config sources";
}

ConfigManager::ConfigManager()
    : startingDirectory(std::filesystem::current_path()) {
    auto env = std::make_unique<ConfigBackendEnv>();
    if (env->load()) {
        backends.emplace_back(std::move(env));
    }
    auto file = std::make_unique<ConfigBackendFile>();
    if (file->load()) {
        backends.emplace_back(std::move(file));
    }
    DLOG(INFO) << "Loaded " << backends.size() << " config sources";
    _argc = -1;
    _argv = nullptr;
}

std::optional<std::string> ConfigManager::get(Configs config) {
    Passes p = Passes::INIT;
    std::string outvalue;
    std::string name = array_helpers::find(kConfigsMap, config)->second;
    name.resize(strlen(name.c_str()));

    p = Passes::FIND_OVERRIDE;
    do {
        switch (p) {
            case Passes::FIND_OVERRIDE:
                for (const auto &bit : backends) {
                    if (bit->doOverride(name)) {
                        DLOG(INFO) << fmt::format(
                            "Used '{}' backend for variable {} (forced)",
                            bit->name(), name);
                        return bit->get(name);
                    }
                }
                p = Passes::ACTUAL_GET;
                break;
            case Passes::ACTUAL_GET:
                for (const auto &bit : backends) {
                    const auto &result = bit->get(name);
                    if (result.has_value()) {
                        DLOG(INFO)
                            << fmt::format("Used '{}' backend for variable {}",
                                           bit->name(), name);
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

void ConfigManager::serializeHelpToOStream(std::ostream &out) {
    out << ConfigBackendCmdline::getTgBotOptionsDesc() << std::endl;
}

char *const *ConfigManager::argv() const { return _argv; }

int ConfigManager::argc() const { return _argc; }

std::filesystem::path ConfigManager::exe() const {
    return startingDirectory / _argv[0];
}