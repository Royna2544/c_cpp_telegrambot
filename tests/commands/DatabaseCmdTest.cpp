#include <tgbot/types/GenericReply.h>
#include <tgbot/types/ReplyKeyboardMarkup.h>

#include <Random.hpp>
#include <memory>

#include "CommandModulesTest.hpp"
#include "api/TgBotApi.hpp"

namespace {
void verifyKeyboard(const TgBot::GenericReply::Ptr& reply) {
    auto keyboardReply =
        std::dynamic_pointer_cast<TgBot::ReplyKeyboardMarkup>(reply);
    ASSERT_TRUE(keyboardReply);

    EXPECT_TRUE(keyboardReply->resizeKeyboard.value());
    EXPECT_TRUE(keyboardReply->oneTimeKeyboard.value());
    EXPECT_TRUE(keyboardReply->selective.value());
    EXPECT_FALSE(keyboardReply->isPersistent.value());
}
}  // namespace

struct DatabaseCommandTest : public CommandTestBase {
    DatabaseCommandTest() : CommandTestBase("database") {}

    template <DatabaseBase::ListType type, DatabaseBase::ListResult result,
              int X, int Y>
    void test_adduser() {
        defaultProvidedMessage->replyToMessage = createDefaultMessage();

        defaultProvidedMessage->replyToMessage->from =
            createDefaultUser(14);  // NOLINT

        constexpr std::string_view something = "sdf";
        EXPECT_CALL(strings, get(Strings::USER_ADDED))
            .WillOnce(Return(something));

        EXPECT_CALL(*database,
                    addUserToList(
                        type, defaultProvidedMessage->replyToMessage->from->id))
            .WillOnce(Return(result));
        test_impl<X, Y>(something);
    }

    template <DatabaseBase::ListType type, DatabaseBase::ListResult result,
              int X, int Y>
    void test_removeuser() {
        defaultProvidedMessage->replyToMessage = createDefaultMessage();
        defaultProvidedMessage->replyToMessage->from =
            createDefaultUser(324);  // NOLINT

        constexpr std::string_view something = "NOSUERE";
        EXPECT_CALL(strings, get(Strings::USER_REMOVED))
            .WillOnce(Return(something));

        EXPECT_CALL(*database,
                    removeUserFromList(
                        type, defaultProvidedMessage->replyToMessage->from->id))
            .WillOnce(Return(result));
        test_impl<X, Y>(something);
    }

    template <int X, int Y, typename Matcher>
    void test_impl(Matcher&& matcher) {
        GenericReply::Ptr reply;
        TgBot::GenericReply::Ptr keyboard;

        const auto sentMessage = createDefaultMessage();
        const auto recievedMessage = createDefaultMessage();
        willSendReplyMessageTo(matcher, recievedMessage, _);
        recievedMessage->replyToMessage = sentMessage;
        EXPECT_CALL(*botApi,
                    sendMessage_impl(
                        TEST_CHAT_ID, _, createMessageReplyMatcher(), _,
                        TgBotApi::parseModeToStr<TgBotApi::ParseMode::None>()))
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
    constexpr std::string_view something = "something";
    EXPECT_CALL(strings, get(Strings::REPLY_TO_USER_MSG))
        .WillOnce(Return(something));
    willSendReplyMessage(something);
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
