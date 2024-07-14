#include <RegEXHandler.hpp>
#include <gtest/gtest.h>

struct RegexHandlerTest : RegexHandlerBase {
    using RegexHandlerBase::doRegexDeleteCommand;
    using RegexHandlerBase::doRegexReplaceCommand;
} inst;

static Message::Ptr createMessage(const std::string& text) {
    auto msgPtr = std::make_shared<Message>();
    msgPtr->text = text;
    return msgPtr;
}

TEST(RegexHandlerTest,
     DoRegexReplaceCommand_ValidInput_ShouldReturnExpectedOutput) {
    const std::string kRegexCommand = "s/a/b/g";
    const std::string kTextToMatch = "aaaaa";
    const std::string kExpectedOutput = "bbbbb";

    const auto msgPtr = createMessage(kRegexCommand);
    const auto actualOutput = inst.doRegexReplaceCommand(msgPtr, kTextToMatch);

    ASSERT_TRUE(actualOutput.has_value());
    EXPECT_EQ(kExpectedOutput, actualOutput.value());
}

TEST(RegexHandlerTest, DoRegexReplaceCommand_InvalidInput_ShouldReturnNullopt) {
    const std::string kRegexCommand = "s/aaaa|bbbb";
    const std::string kTextToMatch = "aaaaa";

    const auto msgPtr = createMessage(kRegexCommand);
    const auto actualOutput = inst.doRegexReplaceCommand(msgPtr, kTextToMatch);

    ASSERT_FALSE(actualOutput.has_value());
}

TEST(RegexHandlerTest,
     DoRegexDeleteCommand_ValidInput_ShouldReturnExpectedOutput) {
    const std::string kRegexCommand = "/aaaa/d";
    const std::string kTextToMatch = "aaaaa\nbbbbb\nccccc\nddddd";
    const std::string kExpectedOutput = "bbbbb\nccccc\nddddd";

    const auto msgPtr = createMessage(kRegexCommand);
    const auto actualOutput = inst.doRegexDeleteCommand(msgPtr, kTextToMatch);

    ASSERT_TRUE(actualOutput.has_value());
    EXPECT_EQ(kExpectedOutput, actualOutput.value());
}

TEST(RegexHandlerTest, DoRegexDeleteCommand_InvalidInput_ShouldReturnNullopt) {
    const std::string kRegexCommand = "/bbbb\\/d";
    const std::string kTextToMatch = "aaaaa\nbbbbb\nccccc\nddddd";

    const auto msgPtr = createMessage(kRegexCommand);
    const auto actualOutput = inst.doRegexDeleteCommand(msgPtr, kTextToMatch);

    ASSERT_FALSE(actualOutput.has_value());
}