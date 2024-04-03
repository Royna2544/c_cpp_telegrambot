#include "DatabaseLoader.h"

#include <absl/log/log.h>
#include <gtest/gtest.h>

#include <mutex>

#include "Database.h"

database::DatabaseWrapperImpl& loadDb() {
    static std::once_flag once;
    static auto& DBWrapper = database::DatabaseWrapperImplObj::getInstance();
    std::call_once(once, [] {
        // DatabaseWrapper::load throws std::runtime_error if it fails to load
        // the database.
        ASSERT_NO_THROW(DBWrapper.load());
        LOG(INFO) << "DB loaded, Owner id " << DBWrapper.maybeGetOwnerId();
    });
    return DBWrapper;
}