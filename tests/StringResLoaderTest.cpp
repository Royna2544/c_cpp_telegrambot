#include <gtest/gtest.h>

#include <api/StringResLoader.hpp>
#include <filesystem>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <limits.h>
#endif

// Helper function to find the strings directory
std::filesystem::path findStringsDirectory() {
    // Resources are installed to <binary_dir>/../share/TgBot++/strings
    // This matches the pattern used by CommandLine::getPath(FS::PathType::RESOURCES)
    
    std::error_code ec;
    std::filesystem::path base_path;
    
#ifdef _WIN32
    // On Windows, get the module filename
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    base_path = std::filesystem::path(path).parent_path();
#elif defined(__linux__)
    // On Linux, read the symlink
    base_path = std::filesystem::read_symlink("/proc/self/exe", ec).parent_path();
    if (ec) {
        base_path = std::filesystem::current_path();
    }
#elif defined(__APPLE__)
    // On macOS, use _NSGetExecutablePath
    char path[PATH_MAX];
    uint32_t size = PATH_MAX;
    if (_NSGetExecutablePath(path, &size) == 0) {
        base_path = std::filesystem::path(path).parent_path();
    } else {
        base_path = std::filesystem::current_path();
    }
#else
    base_path = std::filesystem::current_path();
#endif

    // From the executable directory, go to parent and then share/TgBot++/strings
    return base_path.parent_path() / "share" / "TgBot++" / "strings";
}

class StringResLoaderTest : public ::testing::Test {
   protected:
    std::filesystem::path strings_path;

    void SetUp() override {
        strings_path = findStringsDirectory();
        
        ASSERT_TRUE(std::filesystem::exists(strings_path))
            << "Strings directory not found at: " << strings_path.string();
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
