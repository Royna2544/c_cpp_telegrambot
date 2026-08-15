#include <gtest/gtest.h>

#include "CommandLine.hpp"
#include "DatabaseBase.hpp"
#include "GetCommandLine.hpp"

#ifdef DATABASE_HAVE_PROTOBUF
#include <database/ProtobufDatabase.hpp>
#endif
#ifdef DATABASE_HAVE_SQLITE
#include <database/SQLiteDatabase.hpp>
#endif

#include <fstream>
#include <future>
#include <memory>
#include <string>

struct DBParam {
    std::shared_ptr<DatabaseBase> db;
    std::string_view name;
};

std::ostream& operator<<(std::ostream& os, const DBParam& params) {
    return os << params.name;
}

class DatabaseBaseTest : public ::testing::TestWithParam<DBParam> {
   protected:
    std::shared_ptr<DatabaseBase> db;
    std::filesystem::path db_path = DatabaseBase::kInMemoryDatabase;

    void SetUp() override {
        db = GetParam().db;  // Get the actual implementation
        db->load(db_path);
    }
    void TearDown() override { db->unload(); }
};

// Test addUserToList, checkUserInList, removeUserFromList
TEST_P(DatabaseBaseTest, UserListOperations) {
    UserId user1 = 1001;
    UserId user2 = 1002;

    // Add user1 to whitelist
    EXPECT_EQ(db->addUserToList(DatabaseBase::ListType::WHITELIST, user1),
              DatabaseBase::ListResult::OK);
    // Check user1 is in the whitelist
    EXPECT_EQ(db->checkUserInList(DatabaseBase::ListType::WHITELIST, user1),
              DatabaseBase::ListResult::OK);

    // Add user2 to blacklist
    EXPECT_EQ(db->addUserToList(DatabaseBase::ListType::BLACKLIST, user2),
              DatabaseBase::ListResult::OK);
    // Check user2 is in the blacklist
    EXPECT_EQ(db->checkUserInList(DatabaseBase::ListType::BLACKLIST, user2),
              DatabaseBase::ListResult::OK);

    // Remove user1 from whitelist
    EXPECT_EQ(db->removeUserFromList(DatabaseBase::ListType::WHITELIST, user1),
              DatabaseBase::ListResult::OK);
    // Check user1 is no longer in the whitelist
    EXPECT_EQ(db->checkUserInList(DatabaseBase::ListType::WHITELIST, user1),
              DatabaseBase::ListResult::NOT_IN_LIST);
}

// Test load and unloadDatabase methods
TEST_P(DatabaseBaseTest, LoadAndUnloadDatabase) {
    std::filesystem::path testFilePath = "test_database_file.db";

    // Unload the database
    EXPECT_TRUE(db->unload());

    // Load the database from file
    EXPECT_TRUE(db->load(testFilePath));

    // Unload the database again
    EXPECT_TRUE(db->unload());

    // Remove file
    EXPECT_TRUE(std::filesystem::remove(testFilePath));
}

// Test setting and getting owner user ID
TEST_P(DatabaseBaseTest, SetAndGetOwnerUserId) {
    UserId ownerId = 2001;

    // Set owner user ID
    db->setOwnerUserId(ownerId);

    // Get owner user ID and verify
    auto result = db->getOwnerUserId();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), ownerId);

    EXPECT_EQ(db->claimOwnerUserId(ownerId + 1),
              DatabaseBase::OwnerClaimResult::ALREADY_SET);
}

TEST_P(DatabaseBaseTest, ConcurrentOwnerClaimHasExactlyOneWinner) {
    constexpr UserId firstOwner = 2101;
    constexpr UserId secondOwner = 2102;

    auto first = std::async(std::launch::async, [this] {
        return db->claimOwnerUserId(firstOwner);
    });
    auto second = std::async(std::launch::async, [this] {
        return db->claimOwnerUserId(secondOwner);
    });

    const auto firstResult = first.get();
    const auto secondResult = second.get();
    EXPECT_TRUE((firstResult == DatabaseBase::OwnerClaimResult::OK &&
                 secondResult == DatabaseBase::OwnerClaimResult::ALREADY_SET) ||
                (secondResult == DatabaseBase::OwnerClaimResult::OK &&
                 firstResult == DatabaseBase::OwnerClaimResult::ALREADY_SET));

    const auto owner = db->getOwnerUserId();
    ASSERT_TRUE(owner);
    EXPECT_TRUE(*owner == firstOwner || *owner == secondOwner);
}

TEST_P(DatabaseBaseTest, OwnerCannotBeBlacklisted) {
    constexpr UserId owner = 2201;

    ASSERT_EQ(db->claimOwnerUserId(owner), DatabaseBase::OwnerClaimResult::OK);
    EXPECT_EQ(db->addUserToList(DatabaseBase::ListType::BLACKLIST, owner),
              DatabaseBase::ListResult::ALREADY_IN_OTHER_LIST);
    EXPECT_NE(db->checkUserInList(DatabaseBase::ListType::BLACKLIST, owner),
              DatabaseBase::ListResult::OK);
}

TEST_P(DatabaseBaseTest, BlacklistedUserCannotBecomeOwner) {
    constexpr UserId blacklisted = 2202;

    ASSERT_EQ(db->addUserToList(DatabaseBase::ListType::BLACKLIST, blacklisted),
              DatabaseBase::ListResult::OK);
    EXPECT_EQ(db->claimOwnerUserId(blacklisted),
              DatabaseBase::OwnerClaimResult::BACKEND_ERROR);
    EXPECT_FALSE(db->getOwnerUserId());
}

// Test media info operations
TEST_P(DatabaseBaseTest, MediaInfoOperations) {
    DatabaseBase::MediaInfo mediaInfo1 = {"media1",
                                          "unique1",
                                          {"name1", "name21"},
                                          DatabaseBase::MediaType::PHOTO};
    DatabaseBase::MediaInfo mediaInfo2 = {
        "media2", "unique2", {"name2"}, DatabaseBase::MediaType::VIDEO};

    // Add media info
    EXPECT_EQ(db->addMediaInfo(mediaInfo1), DatabaseBase::AddResult::OK);
    EXPECT_EQ(db->addMediaInfo(mediaInfo2), DatabaseBase::AddResult::OK);

    // Query media info
    auto queriedInfo = db->queryMediaInfo("name1");
    ASSERT_TRUE(queriedInfo.has_value());
    EXPECT_EQ(queriedInfo->mediaId, "media1");

    // Get all media infos
    auto allMedia = db->getAllMediaInfos();
    EXPECT_EQ(allMedia.size(), 2);

    EXPECT_FALSE(db->deleteMediaInfo("does-not-exist"));
    EXPECT_TRUE(db->deleteMediaInfo("media1"));
    EXPECT_FALSE(db->queryMediaInfo("name1").has_value());
}

#ifdef DATABASE_HAVE_SQLITE
TEST(SQLiteDatabaseTest, FailedOpenDoesNotPoisonNextLoad) {
    SQLiteDatabase db(getCmdLine().getPath(FS::PathType::RESOURCES_SQL));
    EXPECT_FALSE(db.load(std::filesystem::temp_directory_path()));
    EXPECT_TRUE(db.load(DatabaseBase::kInMemoryDatabase));
    EXPECT_TRUE(db.unload());
}

TEST(SQLiteDatabaseTest, RejectsUnsupportedFutureSchemaVersion) {
    const auto path = std::filesystem::temp_directory_path() /
                      "glider-sqlite-future-schema-test.db";
    std::filesystem::remove(path);
    sqlite3* raw = nullptr;
    const auto pathString = path.string();
    ASSERT_EQ(sqlite3_open(pathString.c_str(), &raw), SQLITE_OK);
    ASSERT_EQ(
        sqlite3_exec(raw, "PRAGMA user_version=999", nullptr, nullptr, nullptr),
        SQLITE_OK);
    ASSERT_EQ(sqlite3_close(raw), SQLITE_OK);

    SQLiteDatabase db(getCmdLine().getPath(FS::PathType::RESOURCES_SQL));
    EXPECT_FALSE(db.load(path));
    EXPECT_TRUE(db.load(DatabaseBase::kInMemoryDatabase));
    EXPECT_TRUE(db.unload());
    std::filesystem::remove(path);
}

TEST(SQLiteDatabaseTest, MigratesLegacyDuplicateOwnersDeterministically) {
    const auto path = std::filesystem::temp_directory_path() /
                      "glider-sqlite-duplicate-owner-test.db";
    std::filesystem::remove(path);
    sqlite3* raw = nullptr;
    const auto pathString = path.string();
    ASSERT_EQ(sqlite3_open(pathString.c_str(), &raw), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(raw,
                           "CREATE TABLE usermap(userid BIGINT PRIMARY KEY, "
                           "info INT NOT NULL);"
                           "INSERT INTO usermap VALUES(1, 0);"
                           "INSERT INTO usermap VALUES(2, 0);"
                           "PRAGMA user_version=1;",
                           nullptr, nullptr, nullptr),
              SQLITE_OK);
    ASSERT_EQ(sqlite3_close(raw), SQLITE_OK);

    SQLiteDatabase db(getCmdLine().getPath(FS::PathType::RESOURCES_SQL));
    ASSERT_TRUE(db.load(path));
    EXPECT_EQ(db.getOwnerUserId(), 1);
    EXPECT_EQ(db.checkUserInList(DatabaseBase::ListType::WHITELIST, 2),
              DatabaseBase::ListResult::OK);
    EXPECT_EQ(db.claimOwnerUserId(3),
              DatabaseBase::OwnerClaimResult::ALREADY_SET);
    EXPECT_TRUE(db.unload());
    std::filesystem::remove(path);
}
#endif

#ifdef DATABASE_HAVE_PROTOBUF
TEST(ProtoDatabaseTest, ConstructsAndDestructsAcrossLibraryBoundary) {
    std::unique_ptr<DatabaseBase> database = std::make_unique<ProtoDatabase>();
    ASSERT_NE(database, nullptr);
}

TEST(ProtoDatabaseTest, MutationIsDurableBeforeUnload) {
    const auto path = std::filesystem::temp_directory_path() /
                      "glider-protobuf-immediate-persist-test.db";
    std::filesystem::remove(path);

    ProtoDatabase writer;
    ASSERT_TRUE(writer.load(path));
    ASSERT_EQ(writer.claimOwnerUserId(3101),
              DatabaseBase::OwnerClaimResult::OK);
    ASSERT_EQ(writer.addUserToList(DatabaseBase::ListType::WHITELIST, 3102),
              DatabaseBase::ListResult::OK);

    ProtoDatabase reader;
    ASSERT_TRUE(reader.load(path));
    EXPECT_EQ(reader.getOwnerUserId(), 3101);
    EXPECT_EQ(reader.checkUserInList(DatabaseBase::ListType::WHITELIST, 3102),
              DatabaseBase::ListResult::OK);

    EXPECT_TRUE(reader.unload());
    EXPECT_TRUE(writer.unload());
    std::filesystem::remove(path);
}

TEST(ProtoDatabaseTest, ParsedRepeatedFieldsRemainReadableAndMutable) {
    const auto path = std::filesystem::temp_directory_path() /
                      "glider-protobuf-parsed-repeated-field-test.db";
    std::filesystem::remove(path);

    constexpr UserId first = 3200;
    constexpr UserId count = 32;
    {
        glider::proto::database::Database seed;
        seed.set_ownerid(3199);
        auto* const ids = seed.mutable_whitelist()->mutable_id();
        for (UserId offset = 0; offset < count; ++offset) {
            ids->Add(first + offset);
        }

        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(output.is_open());
        ASSERT_TRUE(seed.SerializeToOstream(&output));
    }

    ProtoDatabase reader;
    ASSERT_TRUE(reader.load(path));
    for (UserId offset = 0; offset < count; ++offset) {
        EXPECT_EQ(reader.checkUserInList(DatabaseBase::ListType::WHITELIST,
                                         first + offset),
                  DatabaseBase::ListResult::OK);
    }
    ASSERT_EQ(
        reader.removeUserFromList(DatabaseBase::ListType::WHITELIST, first + 7),
        DatabaseBase::ListResult::OK);
    ASSERT_EQ(
        reader.addUserToList(DatabaseBase::ListType::WHITELIST, first + count),
        DatabaseBase::ListResult::OK);
    ASSERT_TRUE(reader.unload());

    ProtoDatabase verifier;
    ASSERT_TRUE(verifier.load(path));
    EXPECT_EQ(
        verifier.checkUserInList(DatabaseBase::ListType::WHITELIST, first + 7),
        DatabaseBase::ListResult::NOT_IN_LIST);
    EXPECT_EQ(verifier.checkUserInList(DatabaseBase::ListType::WHITELIST,
                                       first + count),
              DatabaseBase::ListResult::OK);
    EXPECT_TRUE(verifier.unload());
    std::filesystem::remove(path);
}

TEST(ProtoDatabaseTest, RepeatedListLookupsDoNotCopyProtobufContainers) {
    ProtoDatabase database;
    ASSERT_TRUE(database.load(DatabaseBase::kInMemoryDatabase));

    constexpr UserId first = 4000;
    constexpr UserId count = 128;
    for (UserId offset = 0; offset < count; ++offset) {
        ASSERT_EQ(database.addUserToList(DatabaseBase::ListType::WHITELIST,
                                         first + offset),
                  DatabaseBase::ListResult::OK);
    }

    for (UserId offset = 0; offset < count; ++offset) {
        EXPECT_EQ(database.checkUserInList(DatabaseBase::ListType::WHITELIST,
                                           first + offset),
                  DatabaseBase::ListResult::OK);
        EXPECT_EQ(database.checkUserInList(DatabaseBase::ListType::BLACKLIST,
                                           first + offset),
                  DatabaseBase::ListResult::ALREADY_IN_OTHER_LIST);
    }
    EXPECT_EQ(database.checkUserInList(DatabaseBase::ListType::WHITELIST,
                                       first + count),
              DatabaseBase::ListResult::NOT_IN_LIST);
    EXPECT_TRUE(database.unload());
}
#endif

// Test chat info operations
TEST_P(DatabaseBaseTest, ChatInfoOperations) {
    ChatId chatId1 = 3001;
    ChatId chatId2 = 3002;
    std::string chatName1 = "chat_name_1";
    std::string chatName2 = "chat_name_2";

    // Add chat info
    EXPECT_EQ(db->addChatInfo(chatId1, chatName1), DatabaseBase::AddResult::OK);
    EXPECT_EQ(db->addChatInfo(chatId2, chatName2), DatabaseBase::AddResult::OK);

    // Get chat ID by name
    auto chatId = db->getChatId(chatName1);
    ASSERT_TRUE(chatId.has_value());
    EXPECT_EQ(chatId.value(), chatId1);

    // Get chat name by ID — must return the name, not the id. Run against both
    // backends, this also guards against the two diverging (the SQLite query
    // previously selected the wrong column).
    auto name1 = db->getChatName(chatId1);
    ASSERT_TRUE(name1.has_value());
    EXPECT_EQ(name1.value(), chatName1);
    auto name2 = db->getChatName(chatId2);
    ASSERT_TRUE(name2.has_value());
    EXPECT_EQ(name2.value(), chatName2);
}

// Test dumping the database to an output stream
TEST_P(DatabaseBaseTest, DumpDatabase) {
    std::ostringstream outputStream;

    // Dump the database to the output stream
    db->dump(outputStream);

    // Validate the output (this will depend on your expected format)
    EXPECT_FALSE(outputStream.str().empty());
}

// Instantiate the parameterized tests with different database implementations
#if defined DATABASE_HAVE_PROTOBUF && defined DATABASE_HAVE_SQLITE
INSTANTIATE_TEST_SUITE_P(
    DatabaseImplementations, DatabaseBaseTest,
    ::testing::Values(
        DBParam{std::make_shared<ProtoDatabase>(), "ProtoDatabase"},
        DBParam{std::make_shared<SQLiteDatabase>(
                    getCmdLine().getPath(FS::PathType::RESOURCES_SQL)),
                "SQLiteDatabase"}));
#elif defined DATABASE_HAVE_PROTOBUF
INSTANTIATE_TEST_SUITE_P(DatabaseImplementations, DatabaseBaseTest,
                         ::testing::Values(DBParam{
                             std::make_shared<ProtoDatabase>(),
                             "ProtoDatabase"}));
#elif defined DATABASE_HAVE_SQLITE
INSTANTIATE_TEST_SUITE_P(
    DatabaseImplementations, DatabaseBaseTest,
    ::testing::Values(DBParam{
        std::make_shared<SQLiteDatabase>(
            getCmdLine().getPath(FS::PathType::RESOURCES_SQL)),
        "SQLiteDatabase"}));
#else
#error "No database backend?"
#endif
