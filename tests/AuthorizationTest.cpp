#include <Authorization.h>
#include <Types.h>
#include <absl/strings/ascii.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <database/bot/TgBotDatabaseImpl.hpp>
#include <memory>

#include "commands/CommandModulesTest.hpp"

struct Param {
    UserId userId{};
    struct {
        bool permissive = false;
        bool requireuser = false;
    } flags;
    struct {
        bool ret = false;
        AuthContext::Result::Reason reason{};
    } expected;
};


constexpr static UserId FAKE_OWNER_ID = 120412;
constexpr static UserId FAKE_BLACKLISTED_ID = 123456;
constexpr static UserId FAKE_WHITELISTED_ID = 1234567890;
constexpr static UserId FAKE_RANDOM_ID = 1234567891;

std::ostream& operator<<(std::ostream& os, const Param& param) {
    switch (param.userId) {
        case FAKE_OWNER_ID:
            os << "Owner";
            break;
        case FAKE_BLACKLISTED_ID:
            os << "BlacklistedUser";
            break;
        case FAKE_WHITELISTED_ID:
            os << "WhitelistedUser";
            break;
        case FAKE_RANDOM_ID:
            os << "RandomUser";
            break;
        default:
            os << "User(" << param.userId << ")";
            break;
    }
    os << " With flags: " << std::boolalpha;
    os << "{ permissive: " << param.flags.permissive;
    os << ", requireuser: " << param.flags.requireuser;
    os << " }, Expected: ret=" << param.expected.ret;
    os << ", reason=" << param.expected.reason;
    return os;
}

class AuthorizationTest : public ::testing::TestWithParam<Param> {
   protected:
    void SetUp() override {
        auto dbinst = TgBotDatabaseImpl::initInstance();
        TgBotDatabaseImpl::Providers provider;
        database = new MockDatabase();
        provider.registerProvider("testing",
                                  std::unique_ptr<MockDatabase>(database));
        ASSERT_TRUE(provider.chooseProvider("testing"));
        ASSERT_TRUE(dbinst->setImpl(std::move(provider)));
        EXPECT_CALL(*database, load(_)).WillOnce(Return(true));
        ASSERT_TRUE(dbinst->load({}));
    }

    void TearDown() override { TgBotDatabaseImpl::destroyInstance(); }

   public:
    static void MakeMessageDateBefore(Message::Ptr& message) {
        using std::chrono_literals::operator""s;

        const auto before =
            std::chrono::system_clock::now() - kMaxTimestampDelay - 1s;
        message->date = std::chrono::system_clock::to_time_t(before);
    }

    MockDatabase* database = nullptr;
};

TEST_P(AuthorizationTest, expectedForMessagesInTime) {
    const auto& param = GetParam();
    auto message = std::make_shared<Message>();
    message->from = std::make_shared<TgBot::User>();
    message->from->id = param.userId;

    // Set up some expectations

    // Returning owner
    ON_CALL(*database, getOwnerUserId).WillByDefault(Return(FAKE_OWNER_ID));
    ON_CALL(*database,
            checkUserInList(DatabaseBase::ListType::WHITELIST, FAKE_OWNER_ID))
        .WillByDefault(Return(DatabaseBase::ListResult::ALREADY_IN_OTHER_LIST));
    ON_CALL(*database,
            checkUserInList(DatabaseBase::ListType::BLACKLIST, FAKE_OWNER_ID))
        .WillByDefault(Return(DatabaseBase::ListResult::ALREADY_IN_OTHER_LIST));

    // Returning blacklisted
    ON_CALL(*database, checkUserInList(DatabaseBase::ListType::BLACKLIST,
                                       FAKE_BLACKLISTED_ID))
        .WillByDefault(Return(DatabaseBase::ListResult::OK));
    ON_CALL(*database, checkUserInList(DatabaseBase::ListType::WHITELIST,
                                       FAKE_BLACKLISTED_ID))
        .WillByDefault(Return(DatabaseBase::ListResult::ALREADY_IN_OTHER_LIST));

    // Returning whitelisted
    ON_CALL(*database, checkUserInList(DatabaseBase::ListType::WHITELIST,
                                       FAKE_WHITELISTED_ID))
        .WillByDefault(Return(DatabaseBase::ListResult::OK));
    ON_CALL(*database, checkUserInList(DatabaseBase::ListType::BLACKLIST,
                                       FAKE_WHITELISTED_ID))
        .WillByDefault(Return(DatabaseBase::ListResult::ALREADY_IN_OTHER_LIST));

    // Returning neither
    ON_CALL(*database,
            checkUserInList(DatabaseBase::ListType::WHITELIST, FAKE_RANDOM_ID))
        .WillByDefault(Return(DatabaseBase::ListResult::NOT_IN_LIST));
    ON_CALL(*database,
            checkUserInList(DatabaseBase::ListType::BLACKLIST, FAKE_RANDOM_ID))
        .WillByDefault(Return(DatabaseBase::ListResult::NOT_IN_LIST));

    // Unloading of database
    ON_CALL(*database, unloadDatabase).WillByDefault(Return(true));

    // Modify date to current date
    message->date =
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    static auto authflags = AuthContext::getInstance();

    unsigned flags = 0;
    if (param.flags.permissive) {
        flags |= AuthContext::PERMISSIVE;
    }
    if (param.flags.requireuser) {
        flags |= AuthContext::REQUIRE_USER;
    }
    const auto& res = authflags->isAuthorized(message, flags);
    if (param.expected.ret) {
        EXPECT_TRUE(res);
        EXPECT_EQ(res.reason, AuthContext::Result::Reason::OK);
    } else {
        EXPECT_FALSE(res);
        EXPECT_EQ(res.reason, param.expected.reason);
    }
}

INSTANTIATE_TEST_SUITE_P(
    AuthorizationTest, AuthorizationTest,
    ::testing::Values(
        // Permissive mode, require user, owner
        Param{FAKE_OWNER_ID,
              {true, true},
              {true, AuthContext::Result::Reason::OK}},

        // Permissive mode, require user, not owner
        Param{FAKE_RANDOM_ID,
              {true, true},
              {true, AuthContext::Result::Reason::OK}},

        // Permissive mode, no require user, owner
        Param{FAKE_OWNER_ID,
              {true, false},
              {true, AuthContext::Result::Reason::OK}},

        // Permissive mode, no require user, not owner
        Param{FAKE_RANDOM_ID,
              {false, false},
              {false, AuthContext::Result::Reason::NOT_IN_WHITELIST}},

        // Non-permissive mode, require user, owner
        Param{FAKE_OWNER_ID,
              {false, true},
              {true, AuthContext::Result::Reason::OK}},

        // Non-permissive mode, require user, not owner
        Param{FAKE_RANDOM_ID,
              {false, true},
              {false, AuthContext::Result::Reason::NOT_IN_WHITELIST}},

        // Non-permissive mode, no require user, owner
        Param{FAKE_OWNER_ID,
              {false, false},
              {true, AuthContext::Result::Reason::OK}},

        // Non-permissive mode, no require user, not owner
        Param{FAKE_RANDOM_ID,
              {false, false},
              {false, AuthContext::Result::Reason::NOT_IN_WHITELIST}},

        // Non-permissive mode, blacklisted user
        Param{FAKE_BLACKLISTED_ID,
              {false, true},
              {false, AuthContext::Result::Reason::NOT_IN_WHITELIST}},

        // Permissive mode, whitelisted user
        Param{FAKE_WHITELISTED_ID,
              {true, false},
              {true, AuthContext::Result::Reason::OK}}));