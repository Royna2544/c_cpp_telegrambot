#include <StringResManager.hpp>
#include <memory>

#include "CommandModulesTest.hpp"
#include "Random.hpp"
#include "gmock/gmock.h"

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
};

TEST_F(DatabaseCommandTest, WithoutUser) {
    setCommandExtArgs();
    willSendReplyMessage(GETSTR(REPLY_TO_USER_MSG));
    execute();
}

TEST_F(DatabaseCommandTest, WithUserAddToWhiteList) {
    Random::initInstance(std::make_unique<MockRandom>());
    defaultProvidedMessage->replyToMessage = createDefaultMessage();

    defaultProvidedMessage->replyToMessage->from =
        createDefaultUser(14);  // NOLINT

    EXPECT_CALL(database,
                addUserToList(DatabaseBase::ListType::WHITELIST,
                              defaultProvidedMessage->replyToMessage->from->id))
        .WillOnce(Return(DatabaseBase::ListResult::OK));

    GenericReply::Ptr reply;
    TgBot::GenericReply::Ptr keyboard;
    constexpr size_t token = 1231;
    const auto sentMessage = createDefaultMessage();
    const auto recievedMessage = createDefaultMessage();
    willSendReplyMessageTo(GETSTR(USER_ADDED), recievedMessage);
    recievedMessage->replyToMessage = sentMessage;
    EXPECT_CALL(*botApi, sendMessage_impl(TEST_CHAT_ID, _,
                                          createMessageReplyMatcher(), _, ""))
        .WillOnce(DoAll(WithArg<3>(verifyKeyboard), SaveArg<3>(&keyboard),
                        Return(sentMessage)));
    EXPECT_CALL(*botApi, registerCallback(_, _))
        .WillOnce(
            DoAll(Invoke([&]() {
                      recievedMessage->text =
                          std::dynamic_pointer_cast<TgBot::ReplyKeyboardMarkup>(
                              keyboard)
                              ->keyboard[0][0]
                              ->text;
                  }),
                  InvokeArgument<0>(botApi, recievedMessage), Return()));
    EXPECT_CALL(*static_cast<MockRandom*>(Random::getInstance()->getImpl()),
                generate(_, _))
        .WillOnce(Return(token));
    EXPECT_CALL(*botApi, unregisterCallback(token)).WillOnce(Return(true));
    execute();
    Random::destroyInstance();
}