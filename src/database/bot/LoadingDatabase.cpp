#include <absl/log/log.h>
#include <absl/strings/str_split.h>

#include <ConfigManager.hpp>
#include <database/bot/TgBotDatabaseImpl.hpp>

#include "DatabaseBase.hpp"

bool TgBotDatabaseImpl_load(ConfigManager* configmgr,
                            TgBotDatabaseImpl* dbimpl) {
    const auto dbConf = configmgr->get(ConfigManager::Configs::DATABASE_CFG);
    bool loaded = false;
    TgBotDatabaseImpl::Providers provider;
    std::pair<std::string, std::filesystem::path> configPair;
    bool configValid = false;

    if (dbConf) {
        // Expected format: <backend>:<filename>
        // Example: sqlite:database.db
        configPair = absl::StrSplit(dbConf.value(), ",");

        if (!provider.chooseProvider(configPair.first)) {
            LOG(ERROR) << "Failed to choose provider";
        } else {
            configValid = true;
        }
    } else {
        LOG(ERROR) << "No database backend specified in config";
    }

    if (!configValid && !provider.chooseAnyProvider()) {
        LOG(ERROR) << "No available database providers";
        return false;
    }
    std::filesystem::path filenameStr =
        configValid ? configPair.second : DatabaseBase::kInMemoryDatabase;
    dbimpl->setImpl(std::move(provider));
    loaded = dbimpl->load(filenameStr);
    if (!loaded) {
        LOG(ERROR) << "Failed to load database";
    } else {
        DLOG(INFO) << "Database loaded";
    }
    return loaded;
}