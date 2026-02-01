#include <absl/log/log.h>
#include <absl/strings/str_split.h>

#include <ConfigManager.hpp>
#include <database/bot/TgBotDatabaseImpl.hpp>

#include "CommandLine.hpp"
#include "DatabaseBase.hpp"

bool TgBotDatabaseImpl_load(ConfigManager* configmgr, TgBotDatabaseImpl* dbimpl,
                            CommandLine* cmdline) {
    const auto dbType = configmgr->get(ConfigManager::Configs::DATABASE_TYPE);
    bool loaded = false;
    TgBotDatabaseImpl::Providers provider(cmdline);
    bool configValid = false;

    if (dbType) {
        if (!provider.chooseProvider(*dbType)) {
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

    const auto dbPath =
        configmgr->get(ConfigManager::Configs::DATABASE_FILEPATH);
    if (dbPath) {
        configValid = true;
    }

    std::filesystem::path filenameStr =
        configValid ? *dbPath : DatabaseBase::kInMemoryDatabase;
    dbimpl->setImpl(std::move(provider));
    LOG(INFO) << "TgbotDatabaseImpl_load: Loading database from "
              << filenameStr.string();
    loaded = dbimpl->load(filenameStr);
    if (!loaded) {
        LOG(ERROR) << "Failed to load database";
    } else {
        DLOG(INFO) << "Database loaded";
    }
    return loaded;
}