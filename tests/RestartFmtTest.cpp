#include <gtest/gtest.h>

#include <stdexcept>

#include "restartfmt_parser.hpp"

TEST(RestartFmtParse, ValidThreeParts) {
    const RestartFmt::Type t("100:200:300");
    EXPECT_EQ(t.chat_id, 100);
    EXPECT_EQ(t.message_id, 200);
    EXPECT_EQ(t.message_thread_id, 300);
}

TEST(RestartFmtParse, NegativeChatId) {
    const RestartFmt::Type t("-1001234567890:5:0");
    EXPECT_EQ(t.chat_id, -1001234567890);
    EXPECT_EQ(t.message_id, 5);
    EXPECT_EQ(t.message_thread_id, 0);
}

TEST(RestartFmtParse, RoundTripThroughToString) {
    const RestartFmt::Type original("100:200:300");
    EXPECT_EQ(RestartFmt::Type(original.to_string()), original);
}

// Regression guards for the out-of-bounds read: when the value does not split
// into exactly three parts, the parser must throw before indexing parts[].
TEST(RestartFmtParse, TooFewPartsThrows) {
    EXPECT_THROW(RestartFmt::Type("100:200"), std::invalid_argument);
}

TEST(RestartFmtParse, SinglePartThrows) {
    EXPECT_THROW(RestartFmt::Type("100"), std::invalid_argument);
}

TEST(RestartFmtParse, EmptyThrows) {
    EXPECT_THROW(RestartFmt::Type(""), std::invalid_argument);
}

TEST(RestartFmtParse, TooManyPartsThrows) {
    EXPECT_THROW(RestartFmt::Type("1:2:3:4"), std::invalid_argument);
}

TEST(RestartFmtParse, NonNumericThrows) {
    EXPECT_THROW(RestartFmt::Type("a:b:c"), std::invalid_argument);
}
