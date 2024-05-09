#include <Authorization.h>
#include <gtest/gtest.h>

#include <chrono>
#include <memory>

#include "DatabaseLoader.h"

static void MakeMessageDateNow(Message::Ptr& message) {
    const auto rn = std::chrono::system_clock::now();
    message->date = std::chrono::system_clock::to_time_t(rn);
}

static void MakeMessageDateBefore(Message::Ptr& message) {
    using std::chrono_literals::operator""s;

    const auto before =
        std::chrono::system_clock::now() - kMaxTimestampDelay - 1s;
    message->date = std::chrono::system_clock::to_time_t(before);
}

static void MakeMessageNonOwner(Message::Ptr& message) {
    message->from = std::make_shared<TgBot::User>();
}

static void MakeMessageOwner(Message::Ptr& message) {
    static UserId ownerId = loadDb().getOwnerUserId();
    MakeMessageNonOwner(message);
    message->from->id = ownerId;
}
static bool Authorized(const Message::Ptr& message, unsigned flags) {
    static auto authflags = AuthContext::getInstance();
    return authflags->isAuthorized(message, flags);
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
    ASSERT_TRUE(Authorized(dummyMsg, AuthContext::Flags::PERMISSIVE));
}

TEST(AuthorizationTest, TimeBeforeOwnerPermissive) {
    auto dummyMsg = std::make_shared<Message>();
    MakeMessageDateBefore(dummyMsg);
    MakeMessageOwner(dummyMsg);
    ASSERT_FALSE(Authorized(dummyMsg, AuthContext::Flags::PERMISSIVE));
}

TEST(AuthorizationTest, TimeNowNonOwnerPermissive) {
    auto dummyMsg = std::make_shared<Message>();
    MakeMessageDateNow(dummyMsg);
    MakeMessageNonOwner(dummyMsg);
    ASSERT_TRUE(Authorized(dummyMsg, AuthContext::Flags::PERMISSIVE));
}

TEST(AuthorizationTest, TimeBeforeNonOwnerPermissive) {
    auto dummyMsg = std::make_shared<Message>();
    MakeMessageDateBefore(dummyMsg);
    MakeMessageNonOwner(dummyMsg);
    ASSERT_FALSE(Authorized(dummyMsg, AuthContext::Flags::PERMISSIVE));
}

TEST(AuthorizationTest, UserRequiredNoUser) {
    auto dummyMsg = std::make_shared<Message>();
    MakeMessageDateNow(dummyMsg);
    ASSERT_FALSE(Authorized(
        dummyMsg, AuthContext::Flags::REQUIRE_USER | AuthContext::Flags::PERMISSIVE));
}

TEST(AuthorizationTest, UserRequiredUser) {
    auto dummyMsg = std::make_shared<Message>();
    MakeMessageDateNow(dummyMsg);
    MakeMessageNonOwner(dummyMsg);
    ASSERT_TRUE(Authorized(
        dummyMsg, AuthContext::Flags::REQUIRE_USER | AuthContext::Flags::PERMISSIVE));
}