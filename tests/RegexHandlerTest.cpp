#include <absl/status/status.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <global_handlers/RegEXHandler.hpp>
#include <memory>
#include <string_view>

#include "TinyStatus.hpp"

using testing::_;

struct RegexHandlerMockInterface : public RegexHandler::Interface {
    MOCK_METHOD(void, onSuccess, (const std::string&), (override));
    MOCK_METHOD(void, onError, (const tinystatus::TinyStatus&), (override));
};

struct Params {
    std::string_view sourceText;
    std::string_view regexCommand;
    std::string_view expectedResult;
};

class RegexHandlerTest : public ::testing::TestWithParam<Params> {
   public:
    std::shared_ptr<RegexHandlerMockInterface> mock =
        std::make_shared<RegexHandlerMockInterface>();
    RegexHandler handler;

    void clearVerification() const {
        testing::Mock::VerifyAndClearExpectations(mock.get());
    }
};

class RegexHandlerTestSuccess : public RegexHandlerTest {};

class RegexHandlerTestFail : public RegexHandlerTest {};

TEST_P(RegexHandlerTestSuccess, TestBody) {
    const auto& param = GetParam();

    std::string result;
    // Expect success
    EXPECT_CALL(*mock, onSuccess(_)).WillOnce(testing::SaveArg<0>(&result));
    // Neither do we expect failure
    EXPECT_CALL(*mock, onError(_)).Times(0);

    handler.execute(mock, param.sourceText.data(), param.regexCommand.data());

    EXPECT_STREQ(param.expectedResult.data(), result.c_str());
    clearVerification();
}

TEST_P(RegexHandlerTestFail, TestBody) {
    const auto& param = GetParam();
    tinystatus::TinyStatus status;

    // Expect failure
    EXPECT_CALL(*mock, onSuccess(_)).Times(0);
    // fail
    EXPECT_CALL(*mock, onError(_)).WillOnce(testing::SaveArg<0>(&status));

    handler.execute(mock, param.sourceText.data(), param.regexCommand.data());
    EXPECT_EQ(status.status(), tinystatus::Status::kInvalidArgument);
    clearVerification();
}

INSTANTIATE_TEST_SUITE_P(
    ExpectedSuccess, RegexHandlerTestSuccess,
    ::testing::Values(
        // Case-insensitive match (i flag)
        Params{"Hello, World!", "s/hello/greeting/i", "greeting, World!"},
        Params{"HELLO, World!", "s/hello/greeting/i", "greeting, World!"},

        // Global match (g flag)
        Params{"Hello, Hello, Hello!", "s/Hello/Hi/g", "Hi, Hi, Hi!"},
        Params{"Hello, hello, Hello!", "s/Hello/Hi/gi", "Hi, Hi, Hi!"},

        // Replace only the second occurrence (number flag)
        Params{"Hello, Hello, Hello!", "s/Hello/Hi/2", "Hello, Hi, Hello!"},
        Params{"Hello, hello, hello!", "s/hello/Hi/2i", "Hello, Hi, hello!"},

        // Mixed: global and case-insensitive
        Params{"hello, Hello, HELLO!", "s/hello/Hi/gi", "Hi, Hi, Hi!"},

        // No flags, first match only
        Params{"Hello, World!", "s/Hello/Hi/", "Hi, World!"},
        Params{"hello, World!", "s/hello/Hi/", "Hi, World!"}));

INSTANTIATE_TEST_SUITE_P(
    ExpectedFailure, RegexHandlerTestFail,
    ::testing::Values(
        // Unbalanced parentheses
        Params{"Hello, World!", "s/Hello(/Hi/", "Exception"},
        Params{"Hello, World!", "s/(Hello/Hi/g", "Exception"},

        // Unbalanced brackets
        Params{"Hello, World!", "s/[Hello/Hi/g", "Exception"},

        // Invalid range in character class
        // Params{"Hello, World!", "s/[z-a]/Hi/g", "Exception"},
        // libc++ doesn't see it as a fault, so remove.

        // Invalid escape sequence (seems not be an error here...)
        // Invalid combination of flags (Skipped as the code would ignore it
        // because of regex)

        // Invalid quantifier
        Params{"Hello, World!", "s/Hello{2,1}/Hi/g",
               "Exception"},  // Invalid range in quantifier

        // Invalid usage of special characters

        // * without preceding valid expression
        Params{"Hello, World!", "s/*Hello/Hi/g", "Exception"},

        // Invalid backreference
        Params{"Hello, World!", "s/(Hello)\\2/Hi/g", "Exception"}
        // Reference to non-existent group
        ));

INSTANTIATE_TEST_SUITE_P(
    ExpectedSuccess_Expanded, RegexHandlerTestSuccess,
    ::testing::Values(
        // --- Replace Command (/s) ---
        // 1. Escaped slash in pattern
        Params{"path/to/file", R"(s/\//-/g)", "path-to-file"},
        // 2. Empty source
        Params{"", "s/test/result/", ""},
        Params{"remove me", "s/remove //", "me"},
        Params{"value: 100", R"(s/(\d+)/[&]/)", "value: [100]"},

        // --- Delete Command (/d) ---
        Params{"line1\ndelete_me\nline3", "/delete_me/d", "line1\nline3\n"},
        Params{"code\n# comment\ncode", "/^#/d", "code\ncode\n"},
        Params{"a\nb\nc", "/.*/d", ""},
        Params{"clean text", "/dirty/d", "clean text\n"},

        // --- Print Command (/p) ---
        Params{"info\nerror: 1\ninfo", "/error/p", "error: 1\n"},
        Params{"a\nb", "/./p", "a\nb\n"}, Params{"a\nb", "/z/p", ""},

        // --- Count Command (/c) ---
        Params{"one two three", "/[a-z]+/c", "3"},
        Params{"a.b.c.d", R"(/\./c)", "3"}, Params{"hello", "/z/c", "0"},

        // --- ToUpper Command (u/) ---
        Params{"hello world", "u/hello/", "HELLO world"},
        Params{"id_a, id_b", "u/id_[a-z]/", "ID_A, ID_B"},
        Params{"failed_state", "u/fail/", "FAILed_state"},
        Params{"unchanged", "u/change/", "unCHANGEd"}));

INSTANTIATE_TEST_SUITE_P(ExpectedFailure_Expanded, RegexHandlerTestFail,
                         ::testing::Values(
                             // 1. Conflicting flags (g + index)
                             Params{"text", "s/t/T/g2", "Exception"},

                             // 2. Invalid Option 'x'.
                             Params{"text", "s/t/T/x", "Exception"},

                             // 3. Match Index Out of Bounds
                             Params{"match", "s/match/replace/5", "Exception"},

                             // --- Syntax Errors ---
                             Params{"text", "/Unbalanced(/d", "Exception"},
                             Params{"text", "/[range/p", "Exception"},
                             Params{"text", "/a{5,1}/c", "Exception"},
                             Params{"text", "u/*start/", "Exception"}));