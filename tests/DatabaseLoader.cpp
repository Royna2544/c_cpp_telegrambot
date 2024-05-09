#include "DatabaseLoader.h"

#include <absl/log/log.h>
#include <gtest/gtest.h>

#include <memory>
#include <mutex>

#include "Authorization.h"

DefaultDatabase& loadDb() {
    static std::once_flag once;
    static auto DBWrapper = std::make_shared<DefaultDatabase>();
    std::call_once(once, [] {
        DBWrapper->loadDatabaseFromFile(
            DefaultBotDatabase::getDatabaseDefaultPath());
        LOG(INFO) << "Database loaded. Owner id is "
                  << DBWrapper->getOwnerUserId();
        AuthContext::initInstance(DBWrapper);
    });
    return *DBWrapper;
}