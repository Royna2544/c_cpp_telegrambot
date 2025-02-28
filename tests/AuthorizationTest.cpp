#include <api/typedefs.h>
#include <absl/strings/ascii.h>
#include <fruit/fruit.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <Authorization.hpp>
#include <chrono>
#include <database/bot/TgBotDatabaseImpl.hpp>
#include <memory>

#include "mocks/DatabaseBase.hpp"

using testing::_;
using testing::Return;

struct AuthParam {
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

std::ostream& operator<<(std::ostream& os, const AuthParam& Authparam) {
    switch (Authparam.userId) {
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
            os << "User(" << Authparam.userId << ")";
            break;
    }
    os << " With flags: " << std::boolalpha;
    os << "{ permissive: " << Authparam.flags.permissive;
    os << ", requireuser: " << Authparam.flags.requireuser;
    os << " }, Expected: ret=" << Authparam.expected.ret;
    os << ", reason=" << Authparam.expected.reason;
    return os;
}

fruit::Component<AuthContext, MockDatabase> getAuthComponent() {
    return fruit::createComponent().bind<DatabaseBase, MockDatabase>();
}

class AuthorizationTest : public ::testing::TestWithParam<AuthParam> {
   protected:
    void SetUp() override {}

   public:
    static void MakeMessageDateBefore(Message::Ptr& message) {
        using std::chrono_literals::operator""s;

        const auto before =
            std::chrono::system_clock::now() - kMaxTimestampDelay - 1s;
        message->date = std::chrono::system_clock::to_time_t(before);
    }

    // The new MockDatabase would be deleted by databaseImpl
    AuthorizationTest()
        : injector(getAuthComponent),
          database(injector.get<MockDatabase*>()),
          auth(injector.get<AuthContext*>()) {}

    fruit::Injector<AuthContext, MockDatabase> injector;
    MockDatabase* database;
    AuthContext* auth;
};

TEST_P(AuthorizationTest, expectedForMessagesInTime) {
    const auto& Authparam = GetParam();
    auto message = std::make_shared<Message>();
    message->from = std::make_shared<TgBot::User>();
    message->from->id = Authparam.userId;

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
    ON_CALL(*database, unload).WillByDefault(Return(true));

    // Modify date to current date
    message->date =
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    AuthContext::Flags flags{};
    if (Authparam.flags.permissive) {
        flags |= AuthContext::Flags::PERMISSIVE;
    }
    if (Authparam.flags.requireuser) {
        flags |= AuthContext::Flags::REQUIRE_USER;
    }
    const auto res = auth->isAuthorized(message, flags);
    if (Authparam.expected.ret) {
        EXPECT_TRUE(res);
        EXPECT_EQ(res.reason, AuthContext::Result::Reason::OK);
    } else {
        EXPECT_FALSE(res);
        EXPECT_EQ(res.reason, Authparam.expected.reason);
    }
}

INSTANTIATE_TEST_SUITE_P(
    AuthorizationTest, AuthorizationTest,
    ::testing::Values(
        // Permissive mode, require user, owner
        AuthParam{FAKE_OWNER_ID,
                  {true, true},
                  {true, AuthContext::Result::Reason::OK}},

        // Permissive mode, require user, not owner
        AuthParam{FAKE_RANDOM_ID,
                  {true, true},
                  {true, AuthContext::Result::Reason::OK}},

        // Permissive mode, no require user, owner
        AuthParam{FAKE_OWNER_ID,
                  {true, false},
                  {true, AuthContext::Result::Reason::OK}},

        // Permissive mode, no require user, not owner
        AuthParam{FAKE_RANDOM_ID,
                  {false, false},
                  {false, AuthContext::Result::Reason::NOT_IN_WHITELIST}},

        // Non-permissive mode, require user, owner
        AuthParam{FAKE_OWNER_ID,
                  {false, true},
                  {true, AuthContext::Result::Reason::OK}},

        // Non-permissive mode, require user, not owner
        AuthParam{FAKE_RANDOM_ID,
                  {false, true},
                  {false, AuthContext::Result::Reason::NOT_IN_WHITELIST}},

        // Non-permissive mode, no require user, owner
        AuthParam{FAKE_OWNER_ID,
                  {false, false},
                  {true, AuthContext::Result::Reason::OK}},

        // Non-permissive mode, no require user, not owner
        AuthParam{FAKE_RANDOM_ID,
                  {false, false},
                  {false, AuthContext::Result::Reason::NOT_IN_WHITELIST}},

        // Non-permissive mode, blacklisted user
        AuthParam{FAKE_BLACKLISTED_ID,
                  {false, true},
                  {false, AuthContext::Result::Reason::NOT_IN_WHITELIST}},

        // Permissive mode, whitelisted user
        AuthParam{FAKE_WHITELISTED_ID,
                  {true, false},
                  {true, AuthContext::Result::Reason::OK}},

        // Permissive mode, blacklisted user
        AuthParam{FAKE_BLACKLISTED_ID,
                  {true, true},
                  {false, AuthContext::Result::Reason::BLACKLISTED_USER}},

        // Non-ermissive mode, whitelisted user
        AuthParam{FAKE_WHITELISTED_ID,
                  {false, false},
                  {true, AuthContext::Result::Reason::OK}}));
