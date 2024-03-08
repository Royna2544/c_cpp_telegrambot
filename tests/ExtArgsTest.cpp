#include <ExtArgs.h>
#include <gtest/gtest.h>
#include <tgbot/types/Message.h>

#include <string>

using TgBot::Message;

constexpr const char kExtArgsSuit1[] = "/start extra_arg1 extra_arg2";
constexpr const char kExtArgsSuit2[] = "/start\n extra_arg1 extra_arg2";
constexpr const char kExtArgsSuit3[] = "/start";
constexpr const char kExtArgsSuit4[] = "";

static Message::Ptr createMessageWithText(const std::string_view& text) {
    std::string extraargs;
    const Message::Ptr message_ptr = std::make_shared<Message>();
    message_ptr->text = text;
    return message_ptr;
}

static void createAndParseExtArgs(const std::string_view& text, std::string& out) {
    parseExtArgs(createMessageWithText(text), out);
}

TEST(ExtArgsTest, ParseExtArgs) {
    std::string extraargs;
    createAndParseExtArgs(kExtArgsSuit1, extraargs);
    EXPECT_EQ(extraargs, "extra_arg1 extra_arg2");
}

TEST(ExtArgsTest, ParseExtArgsNewLine) {
    std::string extraargs;
    createAndParseExtArgs(kExtArgsSuit2, extraargs);
    EXPECT_EQ(extraargs, "extra_arg1 extra_arg2");
}

class ExtArgsTest : public testing::TestWithParam<std::string_view> {
};

class ExtArgsTest2 : public testing::TestWithParam<std::string_view> {
};

TEST_P(ExtArgsTest2, DoesntHaveExtArgs) {
    EXPECT_FALSE(hasExtArgs(createMessageWithText(GetParam())));
}

TEST_P(ExtArgsTest, HasExtArgs) {
    EXPECT_TRUE(hasExtArgs(createMessageWithText(GetParam())));
}

INSTANTIATE_TEST_SUITE_P(HasExtArgs, ExtArgsTest, testing::Values(kExtArgsSuit1, kExtArgsSuit2));
INSTANTIATE_TEST_SUITE_P(DontHaveExtArgs, ExtArgsTest2, testing::Values(kExtArgsSuit3, kExtArgsSuit4));