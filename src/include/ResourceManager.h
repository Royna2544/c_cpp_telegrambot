#pragma once

#include <TgBotUtilsExports.h>

#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "InstanceClassBase.hpp"

struct TgBotUtils_API ResourceManager
    : public InstanceClassBase<ResourceManager>{
    bool preloadOneFile(std::filesystem::path p);
    void preloadResourceDirectory(void);
    const std::string& getResource(std::filesystem::path filename);

    static constexpr auto kResourceDirname = "resources";

   private:
    std::map<std::filesystem::path, std::string> kResources;
    std::vector<std::string> ignoredResources;
    std::once_flag m_once;
    static constexpr char kResourceLoadIgnoreFile[] = ".loadignore";
};
