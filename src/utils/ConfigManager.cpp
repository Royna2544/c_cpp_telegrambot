#include <absl/log/log.h>
#include <fmt/format.h>

#include <ConfigManager.hpp>
#include <algorithm>
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
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>

#include "CommandLine.hpp"
#include "Env.hpp"

namespace po = boost::program_options;

template <typename T, ConfigManager::Configs config>
void AddOption(po::options_description &desc) {
    auto index = std::ranges::find_if(ConfigManager::kConfigMap,
                                      [](const ConfigManager::Entry &entry) {
                                          return entry.config == config;
                                      });
    if constexpr (std::is_same_v<T, void>) {
        if (index->alias != ConfigManager::Entry::ALIAS_NONE) {
            desc.add_options()(
                fmt::format("{},{}", index->name, index->alias).c_str(),
                index->description.data());
        } else {
            desc.add_options()(index->name.data(), index->description.data());
        }
    } else {
        if (index->alias != ConfigManager::Entry::ALIAS_NONE) {
            desc.add_options()(
                fmt::format("{},{}", index->name, index->alias).c_str(),
                po::value<T>(), index->description.data());
        } else {
            desc.add_options()(index->name.data(), po::value<T>(),
                               index->description.data());
        }
    }
}

struct ConfigBackendEnv : public ConfigManager::Backend {
    ~ConfigBackendEnv() override = default;
    ConfigBackendEnv() = default;

    std::optional<std::string> get(const std::string_view name) override {
        Env env;
        if (env[name].has()) {
            return env[name].get();
        }
        return std::nullopt;
    }

    [[nodiscard]] std::string_view name() const override { return "Env"; }
};

template <ConfigManager::Entry::ArgType type>
struct ArgTypeDeducer {};

template <>
struct ArgTypeDeducer<ConfigManager::Entry::ArgType::STRING> {
    using Type = std::string;
};

template <>
struct ArgTypeDeducer<ConfigManager::Entry::ArgType::NONE> {
    using Type = void;
};

template <ConfigManager::Configs config>
void verifyUniqueConfig() {
    constexpr auto count = std::ranges::count_if(
        ConfigManager::kConfigMap,
        [](const auto &entry) { return entry.config == config; });
    static_assert(count == 1,
                  "kConfigMap must and only contain one of each configs");
}

template <size_t index>
void addIndexConfig(po::options_description &desc) {
    constexpr auto config = static_cast<ConfigManager::Configs>(index);

    // Handle special cases.
    if constexpr (config == ConfigManager::Configs::HELP ||
                  config == ConfigManager::Configs::MAX) {
        return;
    }

    constexpr auto argtype =
        std::ranges::find_if(ConfigManager::kConfigMap, [](const auto &entry) {
            return entry.config == config;
        })->type;
    verifyUniqueConfig<config>();
    using ArgType = ArgTypeDeducer<argtype>::Type;
    AddOption<ArgType, config>(desc);
}

template <size_t... index>
void addAll(po::options_description &desc,
            const std::index_sequence<index...> indexs) {
    (addIndexConfig<index>(desc), ...);
}

struct ConfigBackendBoostPOBase : public ConfigManager::Backend {
    static po::options_description getTgBotOptionsDesc() {
        static po::options_description desc("TgBot++ Configs");
        static bool once = [] {
            addAll(desc, std::make_index_sequence<ConfigManager::CONFIG_MAX>());
            return true;
        }();
        return desc;
    }

    std::optional<std::string> get(const std::string_view name) override {
        if (const auto it = mp[std::string(name)]; !it.empty()) {
            return it.as<std::string>();
        }
        return std::nullopt;
    }

    ConfigBackendBoostPOBase() = default;
    ~ConfigBackendBoostPOBase() override = default;

   protected:
    po::variables_map mp;
};

struct ConfigBackendFile : public ConfigBackendBoostPOBase {
    static constexpr const char *kTgBotConfigFiles[] = {
        "tgbotserver." BUILD_TYPE_STR ".ini", "tgbotserver.ini"};
    bool load() override {
        std::filesystem::path home;

        if (!FS::getHomePath(home)) {
            return false;
        }

        std::ifstream ifs;
	std::filesystem::path confPath;
        for (const char *filename : kTgBotConfigFiles) {
            confPath = home / filename;
            ifs.open(confPath);
            if (ifs.fail()) {
                LOG(WARNING) << "Opening " << confPath << " failed";
            } else
                break;
        }
        if (ifs.fail()) {
            LOG(ERROR) << "No suitable config file found";
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
    CommandLine _line;

    static po::options_description getTgBotOptionsDesc() {
        auto desc = ConfigBackendBoostPOBase::getTgBotOptionsDesc();

        AddOption<void, ConfigManager::Configs::HELP>(desc);
        return desc;
    }

    bool load() override {
        try {
            po::store(po::parse_command_line(_line.argc(), _line.argv(),
                                             getTgBotOptionsDesc()),
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

    explicit ConfigBackendCmdline(CommandLine line) : _line(std::move(line)) {}
    ~ConfigBackendCmdline() override = default;
};

ConfigManager::ConfigManager(CommandLine line) {
    auto cmdline = std::make_unique<ConfigBackendCmdline>(std::move(line));
    if (cmdline->load()) {
        storage[BackendType::COMMAND_LINE] = std::move(cmdline);
    }
    auto env = std::make_unique<ConfigBackendEnv>();
    if (env->load()) {
        storage[BackendType::ENV] = std::move(env);
    }
    auto file = std::make_unique<ConfigBackendFile>();
    if (file->load()) {
        storage[BackendType::FILE] = std::move(file);
    }
    DLOG(INFO) << "Loaded " << storage.size() << " config sources";
}

std::optional<std::string> ConfigManager::get(Configs config) {
    std::string_view name =
        std::ranges::find_if(kConfigMap, [config](const Entry &entry) {
            return entry.config == config;
        })->name;

    for (const auto &bit : storage) {
        if (!bit) {
            continue;
        }
        const auto &result = bit->get(name);
        if (result.has_value()) {
            DLOG(INFO) << fmt::format("Used '{}' backend for variable {}",
                                      bit->name(), name);
            return result;
        }
    }

    return std::nullopt;
}

void ConfigManager::serializeHelpToOStream(std::ostream &out) {
    out << ConfigBackendCmdline::getTgBotOptionsDesc() << std::endl;
}
