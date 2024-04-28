#include "DatabaseLoader.h"

#include <absl/log/log.h>
#include <gtest/gtest.h>

#include <mutex>

DefaultDatabase& loadDb() {
    static std::once_flag once;
    static auto DBWrapper = DefaultDatabase();
    std::call_once(once, [] {
        DBWrapper.loadDatabaseFromFile(
            DefaultBotDatabase::getDatabaseDefaultPath());
        LOG(INFO) << "Database loaded. Owner id is " << DBWrapper.getOwnerUserId();
    });
    return DBWrapper;
}