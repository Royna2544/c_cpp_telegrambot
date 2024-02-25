#include <ConfigManager.h>
#include <FileSystemLib.h>
#include <Logging.h>

#include <boost/program_options.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>

struct ConfigBackendBase {
    std::function<void *(void)> load;
    std::function<bool(const void *priv, const std::string &name, std::string &outvalue)> getVariable;
    std::function<bool(const void *priv, const std::string& config)> doOverride;
    const char *name;
    struct {
        void *data = nullptr;
        bool initialized = false;
    } priv;
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
    constexpr static const char kConfigOverrideVar[] = "OVERRIDE_CONF";
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
        ("TOKEN", "Bot Token")
        ("SRC_ROOT", "Root directory of source tree")
        ("PATH", "Enviroment variable PATH")
        (file_priv::kConfigOverrideVar, po::value<std::vector<std::string>>()->multitoken(),
            "Config list to override to file");
    po::store(po::parse_config_file(ifs, desc), p.mp);
    po::notify(p.mp);
    
    LOG_I("Loaded %zu entries from %s", p.mp.size(), confPath.c_str());
    return &p;
}

static bool file_getVariable(const void *p, const std::string &name, std::string &outvalue) {
    auto priv = reinterpret_cast<const file_priv *>(p);
    if (priv && name != file_priv::kConfigOverrideVar) {
        if (const auto it = priv->mp[name]; !it.empty()) {
            outvalue = it.as<std::string>();
            return true;
        }
    }
    return false;
}

static bool file_doOverride(const void *p, const std::string& name) {
    auto priv = reinterpret_cast<const file_priv *>(p);
    if (priv) {
        if (const auto it = priv->mp[file_priv::kConfigOverrideVar]; !it.empty()) {
            const auto vec = it.as<std::vector<std::string>>();
            return std::find(vec.begin(), vec.end(), name) != vec.end();
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
        .doOverride = file_doOverride,
        .name = "File",
    }};

namespace ConfigManager {

bool getVariable(const std::string &name, std::string &outvalue) {
    ConfigBackendBase *ptr;
    int once_flag = 0;

    do {
        for (size_t i = 0; i < sizeof(backends) / sizeof(ConfigBackendBase); ++i) {
            ptr = &backends[i];
            if (!ptr->priv.initialized) {
                if (ptr->load)
                    ptr->priv.data = ptr->load();
                ptr->priv.initialized = true;
            }
            ASSERT(ptr->getVariable, "Bad: getVariable not yet set");
            if (once_flag == 0) {
                if (ptr->doOverride && ptr->doOverride(ptr->priv.data, name)) {
                    LOG_V("Used '%s' backend for fetching var '%s' (forced)", ptr->name, name.c_str());
                    return ptr->getVariable(ptr->priv.data, name, outvalue);
                }
                continue;
            }
            if (ptr->getVariable(ptr->priv.data, name, outvalue)) {
                LOG_V("Used '%s' backend for fetching var '%s'", ptr->name, name.c_str());
                return true;
            }
        }
        ++once_flag;
    } while (once_flag < 2);

    return false;
}
}  // namespace ConfigManager
