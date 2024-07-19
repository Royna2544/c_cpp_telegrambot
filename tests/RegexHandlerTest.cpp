#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <RegEXHandler.hpp>
#include <memory>

using testing::_;

struct RegexHandlerMockInterface : public RegexHandlerBase::Interface {
    MOCK_METHOD(void, onSuccess, (const std::string&), (override));
    MOCK_METHOD(void, onError, (const absl::Status&), (override));
};

class RegexHandlerTest : public ::testing::Test, protected RegexHandlerBase {
   public:
    RegexHandlerTest()
        : RegexHandlerBase(std::make_shared<RegexHandlerMockInterface>()) {}
    
    [[nodiscard]] std::shared_ptr<RegexHandlerMockInterface> getMock() const {
        return std::static_pointer_cast<RegexHandlerMockInterface>(_interface);
    }
};

TEST_F(RegexHandlerTest,
       DoRegexReplaceCommand_ValidInput_ShouldReturnExpectedOutput) {
    const std::string kRegexCommand = "s/a/b/g";
    const std::string kTextToMatch = "aaaaa";
    const std::string kExpectedOutput = "bbbbb";

    setContext({.regexCommand = kRegexCommand, .text = kTextToMatch});

    // OnSuccess isn't called when accessing directly...
    EXPECT_CALL(*getMock(), onSuccess(_)).Times(0);
    // Neither do we expect failure
    EXPECT_CALL(*getMock(), onError(_)).Times(0);

    const auto actualOutput = doRegexReplaceCommand();
    ASSERT_TRUE(actualOutput.has_value());
    EXPECT_EQ(kExpectedOutput, actualOutput.value());

    // Now let's try the public API
    // We expect onSuccess to be called with the expected output
    EXPECT_CALL(*getMock(), onSuccess(kExpectedOutput)).Times(1);
    // We don't expect onFailure to be called
    EXPECT_CALL(*getMock(), onError(_)).Times(0);
    // Call main method
    process();
}

TEST_F(RegexHandlerTest,
       DoRegexReplaceCommand_InvalidInput_ShouldReturnNullopt) {
    const std::string kRegexCommand = "s/aaaa|bbbb";
    const std::string kTextToMatch = "aaaaa";

    setContext({.regexCommand = kRegexCommand, .text = kTextToMatch});

    // OnSuccess shouldn't be called
    EXPECT_CALL(*getMock(), onSuccess(_)).Times(0);
    // Neither onError should be called
    EXPECT_CALL(*getMock(), onError(_)).Times(0);
    const auto actualOutput = doRegexReplaceCommand();

    ASSERT_FALSE(actualOutput.has_value());

    // Now let's try the public API
    // We do not expect onSuccess not to be called
    EXPECT_CALL(*getMock(), onSuccess(_)).Times(0);
    // Neither we expect onError would be called (As it will be noop)
    EXPECT_CALL(*getMock(), onError(_)).Times(0);
    // Call main method
    process();
}

TEST_F(RegexHandlerTest,
       DoRegexDeleteCommand_ValidInput_ShouldReturnExpectedOutput) {
    const std::string kRegexCommand = "/aaaa/d";
    const std::string kTextToMatch = "aaaaa\nbbbbb\nccccc\nddddd";
    const std::string kExpectedOutput = "bbbbb\nccccc\nddddd";

    // OnSuccess isn't called when accessing directly...
    EXPECT_CALL(*getMock(), onSuccess(_)).Times(0);
    // Neither do we expect failure (Regex itself isn't wrong)
    EXPECT_CALL(*getMock(), onError(_)).Times(0);

    setContext({.regexCommand = kRegexCommand, .text = kTextToMatch});
    const auto actualOutput = doRegexDeleteCommand();

    ASSERT_TRUE(actualOutput.has_value());
    EXPECT_EQ(kExpectedOutput, actualOutput.value());

    // Now let's try the public API
    // We expect onSuccess to be called with the expected output
    EXPECT_CALL(*getMock(), onSuccess(kExpectedOutput)).Times(1);
    // We don't expect onFailure to be called
    EXPECT_CALL(*getMock(), onError(_)).Times(0);
    // Call main method
    process();
}

TEST_F(RegexHandlerTest,
       DoRegexDeleteCommand_InvalidInput_ShouldReturnNullopt) {
    const std::string kRegexCommand = "/bbbb\\/d";
    const std::string kTextToMatch = "aaaaa\nbbbbb\nccccc\nddddd";

    setContext({.regexCommand = kRegexCommand, .text = kTextToMatch});
    // In this case, regex itself is invalid - it would call onError...
    EXPECT_CALL(*getMock(), onError(_)).Times(1);
    // OnSuccess shouldn't be called
    EXPECT_CALL(*getMock(), onSuccess(_)).Times(0);

    const auto actualOutput = doRegexDeleteCommand();

    ASSERT_FALSE(actualOutput.has_value());
}