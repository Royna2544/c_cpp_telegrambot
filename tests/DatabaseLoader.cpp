#include "DatabaseLoader.h"

#include <gtest/gtest.h>

#include <cinttypes>
#include <mutex>

#include "Database.h"
#include "Logging.h"

database::DatabaseWrapperImpl& loadDb() {
    static std::once_flag once;
    static auto& DBWrapper = database::DatabaseWrapperImplObj::getInstance();
    std::call_once(once, [] {
        // DatabaseWrapper::load throws std::runtime_error if it fails to load
        // the database.
        ASSERT_NO_THROW(DBWrapper.load());
        LOG(LogLevel::INFO, "DB loaded, Owner id %" PRId64,
            DBWrapper.maybeGetOwnerId());
    });
    return DBWrapper;
}