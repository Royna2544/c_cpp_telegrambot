#include <ConfigManager.h>
#include <Logging.h>
#include <FileSystemLib.h>

#include <cstdlib>
#include <fstream>
#include <functional>
#include <map>

struct ConfigBackendBase {
    std::function<void *(void)> load;
    std::function<bool(const void *priv, const std::string &name, std::string &outvalue)> getVariable;
    const char *name;
    void *priv;
};

static void *env_load(void) { return nullptr; }

static bool env_getVariable(const void *, const std::string &name, std::string &outvalue) {
    char *buf = getenv(name.c_str());
    if (buf) {
        outvalue = buf;
        return true;
    }
    return false;
}

struct file_priv {
    std::map<std::string, std::string> kConfigEntries;
};

static void *file_load(void) {
    std::string home, line;
    static file_priv p{};
    size_t count = 0;

    if (!getHomePath(home)) {
        LOG_E("Cannot find HOME");
        return nullptr;
    }
    const std::string confPath = home + dir_delimiter + ".tgbot_config";
    std::ifstream ifs(confPath);
    if (ifs.fail()) {
        LOG_E("Opening %s failed", confPath.c_str());
        return nullptr;
    }
    while (std::getline(ifs, line)) {
        std::string::size_type pos;
	count++;

        if (pos = line.find('='); pos == std::string::npos) {
            LOG_E("Invalid line in config file: %ld", count);
            continue;
        }
        p.kConfigEntries.emplace(line.substr(0, pos), line.substr(pos + 1));
        LOG_D("%s is '%s'", line.substr(0, pos).c_str(), line.substr(pos + 1).c_str());
    }
    LOG_I("Loaded %ld entries from %s", p.kConfigEntries.size(), confPath.c_str());
    return &p;
}

static bool file_getVariable(const void *p, const std::string &name, std::string &outvalue) {
    auto priv = reinterpret_cast<const file_priv *>(p);
    if (priv) {
        auto it = priv->kConfigEntries.find(name);
        if (it != priv->kConfigEntries.end()) {
            outvalue = it->second;
            return true;
        }
    }
    return false;
}

static struct ConfigBackendBase backends[] = {
    {
        .load = env_load,
        .getVariable = env_getVariable,
	.name = "Env",
    },
    {
        .load = file_load,
        .getVariable = file_getVariable,
	.name = "File",
    }
};

namespace ConfigManager {

void load(void) {
    for (size_t i = 0; i < sizeof(backends) / sizeof(ConfigBackendBase); ++i) {
        backends[i].priv = backends[i].load();
    }
}

bool getVariable(const std::string &name, std::string &outvalue) {
    ConfigBackendBase* ptr;
    for (size_t i = 0; i < sizeof(backends) / sizeof(ConfigBackendBase); ++i) {
        ptr = &backends[i];
        if (ptr->getVariable(ptr->priv, name, outvalue)) {
           LOG_D("Used '%s' backend for fetching var '%s'", ptr->name, name.c_str());
           return true;
	}
    }
    return false;
}
}  // namespace ConfigManager
