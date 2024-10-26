#pragma once

#include <TgBotUtilsExports.h>
#include <trivial_helpers/fruit_inject.hpp>

#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

struct TgBotUtils_API ResourceManager{
    bool preloadOneFile(std::filesystem::path p);
    void preloadResourceDirectory(void);
    std::string_view getResource(std::filesystem::path filename) const;

    APPLE_INJECT(ResourceManager()) {
        preloadResourceDirectory();
    }
    static constexpr auto kResourceDirname = "resources";

   private:
    std::map<std::filesystem::path, std::string> kResources;
    std::vector<std::string> ignoredResources;
    std::once_flag m_once;
    static constexpr std::string_view kResourceLoadIgnoreFile = ".loadignore";
};
