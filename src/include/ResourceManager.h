#pragma once

#include <map>
#include <string>
#include <filesystem>

#include "InstanceClassBase.hpp"

struct ResourceManager : public InstanceClassBase<ResourceManager> {
    bool preloadOneFile(std::filesystem::path p);
    void preloadResourceDirectory(void);
    const std::string& getResource(std::filesystem::path filename);
   private:
    std::map<std::filesystem::path, std::string> kResources;
    const static inline std::string empty;
};
