#include <gtest/gtest.h>

#include <database/ProtobufDatabase.hpp>
#include <database/SQLiteDatabase.hpp>
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
    std::filesystem::path db_path = SQLiteDatabase::kInMemoryDatabase;

    void SetUp() override {
        db = GetParam().db;  // Get the actual implementation
        db->load(db_path);
    }
    void TearDown() override { db->unloadDatabase(); }
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
    EXPECT_TRUE(db->unloadDatabase());

    // Load the database from file
    EXPECT_TRUE(db->load(testFilePath));
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

    // Trying to set owner ID again should fail or be rejected, depending on
    // your implementation
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
    EXPECT_TRUE(db->addMediaInfo(mediaInfo1));
    EXPECT_TRUE(db->addMediaInfo(mediaInfo2));

    // Query media info
    auto queriedInfo = db->queryMediaInfo("name1");
    ASSERT_TRUE(queriedInfo.has_value());
    EXPECT_EQ(queriedInfo->mediaId, "media1");

    // Get all media infos
    auto allMedia = db->getAllMediaInfos();
    EXPECT_EQ(allMedia.size(), 2);
}

// Test chat info operations
TEST_P(DatabaseBaseTest, ChatInfoOperations) {
    ChatId chatId1 = 3001;
    ChatId chatId2 = 3002;
    std::string chatName1 = "chat_name_1";
    std::string chatName2 = "chat_name_2";

    // Add chat info
    EXPECT_TRUE(db->addChatInfo(chatId1, chatName1));
    EXPECT_TRUE(db->addChatInfo(chatId2, chatName2));

    // Get chat ID by name
    auto chatId = db->getChatId(chatName1);
    ASSERT_TRUE(chatId.has_value());
    EXPECT_EQ(chatId.value(), chatId1);
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
INSTANTIATE_TEST_SUITE_P(
    DatabaseImplementations, DatabaseBaseTest,
    ::testing::Values(
        DBParam{std::make_shared<ProtoDatabase>(), "ProtoDatabase"},
        DBParam{std::make_shared<SQLiteDatabase>(), "SQLiteDatabase"}));