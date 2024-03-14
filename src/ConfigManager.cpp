#include <ConfigManager.h>
#include <Logging.h>

#include <boost/program_options.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <libos/libfs.hpp>
#include <mutex>
#include <optional>

struct ConfigBackendBase {
    std::function<void *(void)> load;
    std::function<bool(const void *priv, const std::string &name,
                       std::string &outvalue)>
        getVariable;
    std::function<bool(const void *priv, const std::string &config)> doOverride;
    const char *name;
    struct {
        void *data = nullptr;
        bool initialized = false;
    } priv;
};

static bool env_getVariable(const void *, const std::string &name,
                            std::string &outvalue) {
    char *buf = getenv(name.c_str());
    if (buf) {
        outvalue = buf;
        return true;
    }
    return false;
}

namespace po = boost::program_options;

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

struct cmdline_priv : file_priv {
    static po::options_description getTgBotOptionsDesc() {
        auto desc = file_priv::getTgBotOptionsDesc();
        desc.add_options()("help,h", "Help message for this bot vars");
        return desc;
    }
};

static void *file_load(void) {
    std::filesystem::path home;
    std::string line;
    static file_priv p{};

    if (!getHomePath(home)) {
        LOG(LogLevel::ERROR, "Cannot find HOME");
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

static void *cmdline_load() {
    static cmdline_priv p{};
    int argc = 0;
    const char **argv = nullptr;

    copyCommandLine(CommandLineOp::GET, &argc, &argv);
    if (!argv) {
        LOG(LogLevel::WARNING,
            "Command line copy failed, probably it wasn't saved before");
        return nullptr;
    }

    po::store(
        po::parse_command_line(argc, argv, cmdline_priv::getTgBotOptionsDesc()),
        p.mp);
    po::notify(p.mp);

    LOG(LogLevel::INFO, "Loaded %zu entries", p.mp.size());
    return &p;
}

static bool boost_progopt_getVariable(const void *p, const std::string &name,
                                      std::string &outvalue) {
    auto priv = reinterpret_cast<const file_priv *>(p);
    if (priv && name != file_priv::kConfigOverrideVar) {
        if (const auto it = priv->mp[name]; !it.empty()) {
            outvalue = it.as<std::string>();
            return true;
        }
    }
    return false;
}

static bool boost_progopt_doOverride(const void *p, const std::string &name) {
    auto priv = reinterpret_cast<const file_priv *>(p);
    if (priv) {
        if (const auto it = priv->mp[file_priv::kConfigOverrideVar];
            !it.empty()) {
            const auto vec = it.as<std::vector<std::string>>();
            return std::find(vec.begin(), vec.end(), name) != vec.end();
        }
    }
    return false;
}

void copyCommandLine(CommandLineOp op, int *argc, const char ***argv) {
    static int argc_internal = 0;
    static const char **argv_internal = nullptr;

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

static struct ConfigBackendBase backends[] = {
    {
        .load = cmdline_load,
        .getVariable = boost_progopt_getVariable,
        .doOverride = boost_progopt_doOverride,
        .name = "Command line",
    },
    {
        .load = file_load,
        .getVariable = boost_progopt_getVariable,
        .doOverride = boost_progopt_doOverride,
        .name = "File",
    },
    {
        .getVariable = env_getVariable,
        .name = "Env",
    },
};

namespace ConfigManager {

std::optional<std::string> getVariable(const std::string &name) {
    ConfigBackendBase *ptr;
    int once_flag = 0;
    std::string outvalue;

    do {
        for (size_t i = 0; i < sizeof(backends) / sizeof(ConfigBackendBase);
             ++i) {
            ptr = &backends[i];
            if (!ptr->priv.initialized) {
                if (ptr->load) ptr->priv.data = ptr->load();
                ptr->priv.initialized = true;
            }
            ASSERT(ptr->getVariable, "Bad: getVariable not yet set");
            if (once_flag == 0) {
                if (ptr->doOverride && ptr->doOverride(ptr->priv.data, name)) {
                    LOG(LogLevel::VERBOSE,
                        "Used '%s' backend for fetching var '%s' (forced)",
                        ptr->name, name.c_str());
                    ptr->getVariable(ptr->priv.data, name, outvalue);
                    return {outvalue};
                }
                continue;
            }
            if (ptr->getVariable(ptr->priv.data, name, outvalue)) {
                LOG(LogLevel::VERBOSE,
                    "Used '%s' backend for fetching var '%s'", ptr->name,
                    name.c_str());
                return {outvalue};
            }
        }
        ++once_flag;
    } while (once_flag < 2);

    return std::nullopt;
}

void printHelp() {
    std::cout << cmdline_priv::getTgBotOptionsDesc() << std::endl;
}

}  // namespace ConfigManager
