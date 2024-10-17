#include <Random.hpp>
#include <StringResManager.hpp>
#include <memory>

#include "CommandModulesTest.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "tgbot/types/GenericReply.h"

namespace {
void verifyKeyboard(const TgBot::GenericReply::Ptr& reply) {
    auto keyboardReply =
        std::dynamic_pointer_cast<TgBot::ReplyKeyboardMarkup>(reply);
    ASSERT_TRUE(keyboardReply);

    EXPECT_TRUE(keyboardReply->resizeKeyboard);
    EXPECT_TRUE(keyboardReply->oneTimeKeyboard);
    EXPECT_TRUE(keyboardReply->selective);
    EXPECT_FALSE(keyboardReply->isPersistent);
}
}  // namespace

class MockRandom : public Random::ImplBase {
   public:
    using ret_type = Random::ret_type;

    MOCK_METHOD(bool, isSupported, (), (const, override));
    MOCK_METHOD(ret_type, generate, (const ret_type min, const ret_type max),
                (const, override));
    MOCK_METHOD(std::string_view, getName, (), (const));
    MOCK_METHOD(void, shuffle, (std::vector<std::string>&), (const));
};

struct DatabaseCommandTest : public CommandTestBase {
    DatabaseCommandTest() : CommandTestBase("database") {}

    template <DatabaseBase::ListType type, DatabaseBase::ListResult result,
              int X, int Y>
    void test_adduser() {
        defaultProvidedMessage->replyToMessage = createDefaultMessage();

        defaultProvidedMessage->replyToMessage->from =
            createDefaultUser(14);  // NOLINT
        EXPECT_CALL(*database,
                    addUserToList(
                        type, defaultProvidedMessage->replyToMessage->from->id))
            .WillOnce(Return(result));
        test_impl<X, Y>(GETSTR(USER_ADDED));
    }

    template <DatabaseBase::ListType type, DatabaseBase::ListResult result,
              int X, int Y>
    void test_removeuser() {
        defaultProvidedMessage->replyToMessage = createDefaultMessage();
        defaultProvidedMessage->replyToMessage->from =
            createDefaultUser(324);  // NOLINT

        EXPECT_CALL(*database,
                    removeUserFromList(
                        type, defaultProvidedMessage->replyToMessage->from->id))
            .WillOnce(Return(result));
        test_impl<X, Y>(GETSTR(USER_REMOVED));
    }

    template <int X, int Y, typename Matcher>
    void test_impl(Matcher&& matcher) {
        GenericReply::Ptr reply;
        TgBot::GenericReply::Ptr keyboard;

        constexpr size_t token = 1231;
        const auto sentMessage = createDefaultMessage();
        const auto recievedMessage = createDefaultMessage();
        willSendReplyMessageTo(matcher, recievedMessage, _);
        recievedMessage->replyToMessage = sentMessage;
        EXPECT_CALL(*botApi,
                    sendMessage_impl(TEST_CHAT_ID, _,
                                     createMessageReplyMatcher(), _, ""))
            .WillOnce(DoAll(WithArg<3>(verifyKeyboard), SaveArg<3>(&keyboard),
                            Return(sentMessage)));
        EXPECT_CALL(*botApi, onAnyMessage(_))
            .WillOnce(DoAll(
                Invoke([&]() {
                    recievedMessage->text =
                        std::dynamic_pointer_cast<TgBot::ReplyKeyboardMarkup>(
                            keyboard)
                            ->keyboard[X][Y]
                            ->text;
                }),
                InvokeArgument<0>(botApi, recievedMessage), Return()));
        execute();
    }
};

TEST_F(DatabaseCommandTest, WithoutUser) {
    setCommandExtArgs();
    willSendReplyMessage(GETSTR(REPLY_TO_USER_MSG));
    execute();
}

TEST_F(DatabaseCommandTest, WithUserAddToWhiteList) {
    test_adduser<DatabaseBase::ListType::WHITELIST,
                 DatabaseBase::ListResult::OK, 0, 0>();
}

TEST_F(DatabaseCommandTest, WithUserRemoveFromWhiteList) {
    test_removeuser<DatabaseBase::ListType::WHITELIST,
                    DatabaseBase::ListResult::OK, 0, 1>();
}

TEST_F(DatabaseCommandTest, WithUserAddToBlackList) {
    test_adduser<DatabaseBase::ListType::BLACKLIST,
                 DatabaseBase::ListResult::OK, 1, 0>();
}

TEST_F(DatabaseCommandTest, WithUserRemoveFromBlackList) {
    test_removeuser<DatabaseBase::ListType::BLACKLIST,
                    DatabaseBase::ListResult::OK, 1, 1>();
}