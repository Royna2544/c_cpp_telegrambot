#include <ConfigManager.h>
#include <FileSystemLib.h>
#include <Logging.h>

#include <boost/program_options.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <mutex>
#include <stdexcept>
#include "StringToolsExt.h"

struct ConfigBackendBase {
    std::function<void *(void)> load;
    std::function<bool(const void *priv, const std::string &name, std::string &outvalue)> getVariable;
    const char *name;
    void *priv;
};

static bool env_getVariable(const void *, const std::string &name, std::string &outvalue) {
    char *buf = getenv(name.c_str());
    if (buf) {
        outvalue = buf;
        return true;
    }
    return false;
}

namespace po = boost::program_options;
struct file_priv {
    po::variables_map mp;    
};

static void *file_load(void) {
    std::filesystem::path home;
    std::string line;
    po::options_description desc("TgBot++ Configs");
    static file_priv p{};
    
    if (!getHomePath(home)) {
        LOG_E("Cannot find HOME");
        return nullptr;
    }
    const auto confPath = (home / ".tgbot_conf.ini").string();
    std::ifstream ifs(confPath);
    if (ifs.fail()) {
        LOG_E("Opening %s failed", confPath.c_str());
        return nullptr;
    }
    desc.add_options()
        ("TOKEN", po::value<std::string>(), "Bot Token")
        ("SRC_ROOT", po::value<std::filesystem::path>(), "Root directory of source tree");
    po::store(po::parse_config_file(ifs, desc), p.mp);
    po::notify(p.mp);
    
    LOG_I("Loaded %zu entries from %s", p.mp.size(), confPath.c_str());
    return &p;
}

static bool file_getVariable(const void *p, const std::string &name, std::string &outvalue) {
    auto priv = reinterpret_cast<const file_priv *>(p);
    if (priv) {
        if (const auto it = priv->mp[name]; !it.empty()) {
            outvalue = it.as<std::string>();
            return true;
        }
    }
    return false;
}

static struct ConfigBackendBase backends[] = {
    {
        .getVariable = env_getVariable,
        .name = "Env",
    },
    {
        .load = file_load,
        .getVariable = file_getVariable,
        .name = "File",
    }};

namespace ConfigManager {

static bool load(void) {
    static std::once_flag once;
    try {
        std::call_once(once, [] {
            for (size_t i = 0; i < sizeof(backends) / sizeof(ConfigBackendBase); ++i) {
                auto ptr = &backends[i];
                if (!ptr->getVariable) throw std::runtime_error("Not ready");
                if (ptr->load)
                    ptr->priv = ptr->load();
                else
                    ptr->priv = nullptr;
            }
        });
    } catch (const std::runtime_error &) {
        return false;
    }
    return true;
}

bool getVariable(const std::string &name, std::string &outvalue) {
    ConfigBackendBase *ptr;

    if (load()) {
        for (size_t i = 0; i < sizeof(backends) / sizeof(ConfigBackendBase); ++i) {
            ptr = &backends[i];
            if (ptr->getVariable(ptr->priv, name, outvalue)) {
                LOG_V("Used '%s' backend for fetching var '%s'", ptr->name, name.c_str());
                return true;
            }
        }
    } else {
        LOG_E("Manager not ready: while loading '%s'", name.c_str());
    }
    return false;
}
}  // namespace ConfigManager
