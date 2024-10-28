#include <gtest/gtest.h>

#include <CompileTimeStringConcat.hpp>
#include <string_view>

using namespace StringConcat;

TEST(StringConcat, BasicStringConcatenation) {
    constexpr auto str1 = createString("Hello");
    constexpr auto str2 = createString("World");
    constexpr auto result = cat(str1, str2);
    static_assert(result.size() == 12);
    static_assert(result[0] == 'H');
    static_assert(result[9] == 'd');
    static_assert(result[10] == '\0');
    EXPECT_STREQ(result.get().data(), "HelloWorld");
}

TEST(StringConcat, OperatorStringView) {
    constexpr auto str1 = createString("Blex bomb");
    constexpr auto str2 = createString('!');
    String<30> result = cat(str1, str2);
    static_assert(cat(str1, str2).size() == 11);
    std::string_view sv = result;
    EXPECT_STREQ(sv.data(), "Blex bomb!");
}

TEST(StringConcat, OperatorString) {
    constexpr auto str1 = createString("Clex bomb");
    constexpr auto str2 = createString('!');
    String<30> result = cat(str1, str2);
    static_assert(cat(str1, str2).size() == 11);
    std::string sv = result;
    EXPECT_STREQ(sv.c_str(), "Clex bomb!");
    EXPECT_EQ(sv.size(), result.size());
}

TEST(StringConcat, OperatorStringForCat) {
    constexpr String<30> str1 = cat("Dlex bomb");
    std::string sv = str1;
    EXPECT_STREQ(sv.c_str(), "Dlex bomb");
    EXPECT_EQ(sv.size(), str1.size());
}