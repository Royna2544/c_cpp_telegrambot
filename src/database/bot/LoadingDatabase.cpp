#include <database/bot/TgBotDatabaseImpl.hpp>
#include <ConfigManager.hpp>
#include <absl/log/log.h>

bool TgBotDatabaseImpl::load(ConfigManager* configmgr, TgBotDatabaseImpl* dbimpl) {
    const auto dbConf = configmgr->get(ConfigManager::Configs::DATABASE_CFG);
    std::error_code ec;
    bool loaded = false;

    if (!dbConf) {
        LOG(ERROR) << "No database backend specified in config";
        return false;
    }

    const std::string& config = dbConf.value();
    const auto speratorIdx = config.find(':');

    if (speratorIdx == std::string::npos) {
        LOG(ERROR) << "Invalid database configuration";
        return false;
    }

    // Expected format: <backend>:filename relative to git root (Could be
    // absolute)
    const auto backendStr = config.substr(0, speratorIdx);
    const auto filenameStr = config.substr(speratorIdx + 1);

    TgBotDatabaseImpl::Providers provider;
    if (!provider.chooseProvider(backendStr)) {
        LOG(ERROR) << "Failed to choose provider";
        return false;
    }
    dbimpl->setImpl(std::move(provider));
    loaded = dbimpl->load(filenameStr);
    if (!loaded) {
        LOG(ERROR) << "Failed to load database";
    } else {
        DLOG(INFO) << "Database loaded";
    }
    return loaded;
}