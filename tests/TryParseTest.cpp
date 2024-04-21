#include <gtest/gtest.h>

#include <TryParseStr.hpp>

TEST(TryParseTest, ParseInteger) {
    int value;
    ASSERT_TRUE(try_parse<int>("123", &value));
    ASSERT_EQ(123, value);
}

TEST(TryParseTest, ParseFloat) {
    float value;
    ASSERT_TRUE(try_parse<float>("123.456", &value));
    ASSERT_EQ(123.456f, value);
}

TEST(TryParseTest, ParseInvalidFloat) {
    int value = 0;
    ASSERT_FALSE(try_parse<int>("123.456a", &value));
    ASSERT_EQ(0, value);
}

TEST(TryParseTest, ParseEmptyString) {
    int value = 0;
    ASSERT_FALSE(try_parse<int>("", &value));
}