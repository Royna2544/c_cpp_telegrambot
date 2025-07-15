#include <absl/strings/ascii.h>
#include <api/typedefs.h>
#include <fruit/fruit.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <api/AuthContext.hpp>
#include <chrono>
#include <database/bot/TgBotDatabaseImpl.hpp>
#include <memory>

#include "mocks/DatabaseBase.hpp"

using testing::_;
using testing::Return;

struct AuthParam {
    UserId userId{};
    AuthContext::AccessLevel level{};
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
    os << " With level: " << Authparam.level;
    os << ", Expected: ret=" << Authparam.expected.ret;
    os << ", reason=" << Authparam.expected.reason;
    return os;
}

fruit::Component<AuthContext, MockDatabase> getAuthComponent() {
    return fruit::createComponent().bind<DatabaseBase, MockDatabase>();
}

class AuthContextTest : public ::testing::TestWithParam<AuthParam> {
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
    AuthContextTest()
        : injector(getAuthComponent),
          database(injector.get<MockDatabase*>()),
          auth(injector.get<AuthContext*>()) {}

    fruit::Injector<AuthContext, MockDatabase> injector;
    MockDatabase* database;
    AuthContext* auth;
};

TEST_P(AuthContextTest, expectedForMessagesInTime) {
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

    const auto res = auth->isAuthorized(message, Authparam.level);
    if (Authparam.expected.ret) {
        EXPECT_TRUE(res);
        EXPECT_EQ(res.result.second, AuthContext::Result::Reason::Ok);
    } else {
        EXPECT_FALSE(res);
        EXPECT_EQ(res.result.second, Authparam.expected.reason);
    }
}

INSTANTIATE_TEST_SUITE_P(
    AuthContextTest, AuthContextTest,
    ::testing::Values(
        // Owner access #AdminUser
        AuthParam{FAKE_OWNER_ID,
                  AuthContext::AccessLevel::AdminUser,
                  {true, AuthContext::Result::Reason::Ok}},

        // RandomUser access #User
        AuthParam{FAKE_RANDOM_ID,
                  AuthContext::AccessLevel::User,
                  {true, AuthContext::Result::Reason::Ok}},

        // Owner access #Unprotected
        AuthParam{FAKE_OWNER_ID,
                  AuthContext::AccessLevel::Unprotected,
                  {true, AuthContext::Result::Reason::Ok}},

        // RandomUser access #Unprotected
        AuthParam{FAKE_RANDOM_ID,
                  AuthContext::AccessLevel::Unprotected,
                  {true, AuthContext::Result::Reason::Ok}},

        // Owner access #AdminUser
        AuthParam{FAKE_OWNER_ID,
                  AuthContext::AccessLevel::AdminUser,
                  {true, AuthContext::Result::Reason::Ok}},

        // RandomUser access #AdminUser
        AuthParam{FAKE_RANDOM_ID,
                  AuthContext::AccessLevel::AdminUser,
                  {false, AuthContext::Result::Reason::PermissionDenied}},

        // Blacklist access #User
        AuthParam{FAKE_BLACKLISTED_ID,
                  AuthContext::AccessLevel::User,
                  {false, AuthContext::Result::Reason::ForbiddenUser}},

        // Whitelist access #AdminUser
        AuthParam{FAKE_WHITELISTED_ID,
                  AuthContext::AccessLevel::AdminUser,
                  {true, AuthContext::Result::Reason::Ok}},

        // Blacklist access #AdminUser
        AuthParam{FAKE_BLACKLISTED_ID,
                  AuthContext::AccessLevel::AdminUser,
                  {false, AuthContext::Result::Reason::ForbiddenUser}},

        // WhiteList access #User
        AuthParam{FAKE_WHITELISTED_ID,
                  AuthContext::AccessLevel::User,
                  {true, AuthContext::Result::Reason::Ok}}));
