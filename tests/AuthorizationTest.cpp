#include <gtest/gtest.h>

#include <Authorization.h>
#include <Database.h>

#include <cinttypes>
#include <chrono>
#include <memory>
#include <mutex>

static database::DatabaseWrapper& loadDb() {
    static std::once_flag once;
    std::call_once(once, []{
        database::DBWrapper.load();
        LOG_I("DB loaded, Owner id %" PRId64, database::DBWrapper.maybeGetOwnerId());
    });
    return database::DBWrapper;
}

static void MakeMessageDateNow(Message::Ptr& message) {
    const auto rn = std::chrono::system_clock::now();
    message->date = std::chrono::system_clock::to_time_t(rn);
}

static void MakeMessageDateBefore(Message::Ptr& message) {
    using std::chrono_literals::operator""s;

    const auto before = std::chrono::system_clock::now()- kMaxTimestampDelay - 1s;
    message->date = std::chrono::system_clock::to_time_t(before);
}

static void MakeMessageNonOwner(Message::Ptr& message) {
    message->from = std::make_shared<TgBot::User>();
}

static void MakeMessageOwner(Message::Ptr& message) {
    static UserId ownerId = loadDb().maybeGetOwnerId();
    MakeMessageNonOwner(message);
    message->from->id = ownerId;
}

TEST(AuthorizationTest, TimeNowOwnerEnforce) {
    auto dummyMsg = std::make_shared<Message>();
    MakeMessageDateNow(dummyMsg);
    MakeMessageOwner(dummyMsg);
    ASSERT_TRUE(Authorized(dummyMsg, 0));
}

TEST(AuthorizationTest, TimeBeforeOwnerEnforce) {
    auto dummyMsg = std::make_shared<Message>();
    MakeMessageDateBefore(dummyMsg);
    MakeMessageOwner(dummyMsg);
    ASSERT_FALSE(Authorized(dummyMsg, 0));
}

TEST(AuthorizationTest, TimeNowNonOwnerEnforce) {
    auto dummyMsg = std::make_shared<Message>();
    MakeMessageDateNow(dummyMsg);
    MakeMessageNonOwner(dummyMsg);
    ASSERT_FALSE(Authorized(dummyMsg, 0));
}

TEST(AuthorizationTest, TimeBeforeNonOwnerEnforce) {
    auto dummyMsg = std::make_shared<Message>();
    MakeMessageDateBefore(dummyMsg);
    MakeMessageNonOwner(dummyMsg);
    ASSERT_FALSE(Authorized(dummyMsg, 0));
}

TEST(AuthorizationTest, TimeNowOwnerPermissive) {
    auto dummyMsg = std::make_shared<Message>();
    MakeMessageDateNow(dummyMsg);
    MakeMessageOwner(dummyMsg);
    ASSERT_TRUE(Authorized(dummyMsg, AuthorizeFlags::PERMISSIVE));
}

TEST(AuthorizationTest, TimeBeforeOwnerPermissive) {
    auto dummyMsg = std::make_shared<Message>();
    MakeMessageDateBefore(dummyMsg);
    MakeMessageOwner(dummyMsg);
    ASSERT_FALSE(Authorized(dummyMsg, AuthorizeFlags::PERMISSIVE));
}

TEST(AuthorizationTest, TimeNowNonOwnerPermissive) {
    auto dummyMsg = std::make_shared<Message>();
    MakeMessageDateNow(dummyMsg);
    MakeMessageNonOwner(dummyMsg);
    ASSERT_TRUE(Authorized(dummyMsg, AuthorizeFlags::PERMISSIVE));
}

TEST(AuthorizationTest, TimeBeforeNonOwnerPermissive) {
    auto dummyMsg = std::make_shared<Message>();
    MakeMessageDateBefore(dummyMsg);
    MakeMessageNonOwner(dummyMsg);
    ASSERT_FALSE(Authorized(dummyMsg, AuthorizeFlags::PERMISSIVE));
}

TEST(AuthorizationTest, UserRequiredNoUser) {
    auto dummyMsg = std::make_shared<Message>();
    MakeMessageDateNow(dummyMsg);
    ASSERT_FALSE(Authorized(dummyMsg, AuthorizeFlags::REQUIRE_USER | AuthorizeFlags::PERMISSIVE));
}

TEST(AuthorizationTest, UserRequiredUser) {
    auto dummyMsg = std::make_shared<Message>();
    MakeMessageDateNow(dummyMsg);
    MakeMessageNonOwner(dummyMsg);
    ASSERT_TRUE(Authorized(dummyMsg, AuthorizeFlags::REQUIRE_USER | AuthorizeFlags::PERMISSIVE));
}