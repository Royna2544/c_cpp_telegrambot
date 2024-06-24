#include <filesystem>
#include <string>
#include <vector>
#include "InstanceClassBase.hpp"

#if __has_include("resources.gen.h")
#include "resources.gen.h"
#endif

// Shorthand macro for getting a string
#define GETSTR(x) StringResManager::getInstance()->getString(STRINGRES_ ##x)
#define GETSTR_IS(x) (GETSTR(x) + ": ")
#define GETSTR_BRACE(x) ("(" + GETSTR(x) + ")")

struct StringResManager : InstanceClassBase<StringResManager> {
    std::vector<std::pair<std::string, std::string>> m_strings;
    
    bool parseFromFile(const std::filesystem::path& path, bool for_generate = false);
    // One of STRINGRES_* constants
    std::string getString(const int key) const;
};