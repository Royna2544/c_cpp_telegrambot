#include <ResourceManager.h>

#include "CommandModulesTest.hpp"
#include "TgBotWrapper.hpp"

class AliveCommandTest : public CommandTestBase {
   public:
    AliveCommandTest() : CommandTestBase("alive") {}
    ~AliveCommandTest() override = default;
    void SetUp() override {
        CommandTestBase::SetUp();
        ResourceManager::getInstance()->initWrapper();
    }
    void TearDown() override {
        CommandTestBase::TearDown();
        ResourceManager::destroyInstance();
    }
};

TEST_F(AliveCommandTest, hasAliveMediaName) {
    setCommandExtArgs();
    const auto botUser = std::make_shared<User>();

    // Would call two times for username, nickname
    EXPECT_CALL(*botApi, getBotUser_impl())
        .Times(2)
        .WillRepeatedly(Return(botUser));

    // First, if alive medianame existed
    EXPECT_CALL(database, queryMediaInfo(ALIVE_FILE_ID))
        .WillOnce(Return(TEST_MEDIAINFO));

    // Expected to pass the fileid and parsemode as HTML
    willSendReplyFile<TgBotWrapper::ParseMode::HTML>(
        TgBotWrapper::FileOrString{std::string{TEST_MEDIA_ID}}, _);
    execute();
}

TEST_F(AliveCommandTest, DoesntHaveAliveMediaName) {
    setCommandExtArgs();
    const auto botUser = std::make_shared<User>();

    // Would call two times for username, nickname
    EXPECT_CALL(*botApi, getBotUser_impl())
        .Times(2)
        .WillRepeatedly(Return(botUser));
    // Second, if alive medianame not existed
    EXPECT_CALL(database, queryMediaInfo(ALIVE_FILE_ID))
        .WillOnce(Return(std::nullopt));

    willSendReplyMessage<TgBotWrapper::ParseMode::HTML>(_);
    execute();
}