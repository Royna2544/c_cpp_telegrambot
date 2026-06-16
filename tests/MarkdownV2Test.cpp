#include <gtest/gtest.h>

#include <api/MarkdownV2.hpp>

using tgbot::markdownv2::escape;

TEST(MarkdownV2Escape, PlainTextUnchanged) {
    EXPECT_EQ(escape("hello world"), "hello world");
}

TEST(MarkdownV2Escape, EscapesSpecialCharacters) {
    EXPECT_EQ(escape("a.b!c-d"), "a\\.b\\!c\\-d");
    EXPECT_EQ(escape("(x)[y]"), "\\(x\\)\\[y\\]");
}

// Backslash must be escaped first, and StrReplaceAll's single pass must not
// re-escape the backslashes it inserts for other characters.
TEST(MarkdownV2Escape, BackslashEscapedFirstNoDoubleEscape) {
    // A literal backslash becomes two backslashes...
    EXPECT_EQ(escape("\\"), "\\\\");
    // ...and a '.' still becomes exactly "\." (not "\\.").
    EXPECT_EQ(escape("."), "\\.");
    // A backslash followed by a dot: each is escaped once, independently.
    EXPECT_EQ(escape("\\."), "\\\\\\.");
}

TEST(MarkdownV2Escape, BacktickEscapedNotDeleted) {
    EXPECT_EQ(escape("`code`"), "\\`code\\`");
}
