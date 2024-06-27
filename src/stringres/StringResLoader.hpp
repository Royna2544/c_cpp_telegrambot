#include <CStringLifetime.h>
#include <ConfigManager.h>

#include <filesystem>
#include <string>
#include <vector>

struct StringResLoader {
    std::vector<std::pair<std::string, std::string>> m_strings;

    bool parseFromFile(const std::filesystem::path& path,
                       int expected_size = 0);
    // One of STRINGRES_* constants
    [[nodiscard]] std::string getString(const int key) const;
};