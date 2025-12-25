#include <gtest/gtest.h>

#include <api/StringResLoader.hpp>
#include <filesystem>
#include <string>
#include <vector>

class StringResLoaderTest : public ::testing::Test {
   protected:
    std::filesystem::path strings_path;

    void SetUp() override {
        // Get the path to the strings directory relative to the test binary
        // The test binaries are in build/, so we need to go up one level to find resources/
        auto test_binary_path = std::filesystem::current_path();
        strings_path = test_binary_path.parent_path() / "resources" / "strings";
        
        // If not found, try relative to the current working directory (for in-tree builds)
        if (!std::filesystem::exists(strings_path)) {
            strings_path = std::filesystem::current_path() / "resources" / "strings";
        }
        
        // If still not found, try one directory up (common for build dirs)
        if (!std::filesystem::exists(strings_path)) {
            strings_path = test_binary_path / ".." / "resources" / "strings";
        }
        
        ASSERT_TRUE(std::filesystem::exists(strings_path))
            << "Strings directory not found. Tried: " << strings_path.string();
    }
};

// Test that all locale files can be loaded without errors
TEST_F(StringResLoaderTest, LoadAllLocales) {
    ASSERT_NO_THROW({
        StringResLoader loader(strings_path);
    });
}

// Test that each expected locale file is accessible
TEST_F(StringResLoaderTest, VerifyLocaleAccess) {
    StringResLoader loader(strings_path);

    // List of expected locales based on the files we've added
    std::vector<std::string> expected_locales = {
        "en",  // English (default)
        "fr",  // French
        "ko",  // Korean
        "es",  // Spanish
        "de",  // German
        "ja",  // Japanese
        "zh",  // Chinese
        "ru",  // Russian
        "pt"   // Portuguese
    };

    // Verify each locale can be accessed
    for (const auto& locale : expected_locales) {
        const auto* locale_map = loader.at(locale);
        ASSERT_NE(locale_map, nullptr) << "Failed to access locale: " << locale;
        
        // Verify we can get a string from each locale
        auto working_on_it = locale_map->get(Strings::WORKING_ON_IT);
        EXPECT_FALSE(working_on_it.empty()) 
            << "Empty string for WORKING_ON_IT in locale: " << locale;
    }
}

// Test that default locale fallback works
TEST_F(StringResLoaderTest, DefaultLocaleFallback) {
    StringResLoader loader(strings_path);

    // Request a non-existent locale, should fall back to default (en)
    const auto* locale_map = loader.at("non_existent_locale");
    ASSERT_NE(locale_map, nullptr);
    
    // Verify we get the English string
    auto working_on_it = locale_map->get(Strings::WORKING_ON_IT);
    EXPECT_EQ(working_on_it, "Working on it...");
}

// Test that all strings are present in each locale
TEST_F(StringResLoaderTest, AllStringsPresent) {
    StringResLoader loader(strings_path);

    std::vector<std::string> locales = {"en", "fr", "ko", "es", "de", "ja", "zh", "ru", "pt"};

    // Test a selection of important strings across all locales
    std::vector<Strings> test_strings = {
        Strings::WORKING_ON_IT,
        Strings::OPERATION_SUCCESSFUL,
        Strings::OPERATION_FAILURE,
        Strings::UNKNOWN_ERROR,
        Strings::USER_ADDED,
        Strings::USER_REMOVED,
        Strings::EXAMPLE
    };

    for (const auto& locale : locales) {
        const auto* locale_map = loader.at(locale);
        ASSERT_NE(locale_map, nullptr) << "Failed to load locale: " << locale;

        for (const auto& string_key : test_strings) {
            auto str = locale_map->get(string_key);
            EXPECT_FALSE(str.empty())
                << "Missing or empty string for " << string_key 
                << " in locale: " << locale;
        }
    }
}
