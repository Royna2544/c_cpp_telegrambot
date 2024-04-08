#pragma once

#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "CStringLifetime.h"
#include "InstanceClassBase.hpp"
#include "initcalls/Initcall.hpp"

struct ResourceManager : public InstanceClassBase<ResourceManager>, InitCall {
    bool preloadOneFile(std::filesystem::path p);
    void preloadResourceDirectory(void);
    const std::string& getResource(std::filesystem::path filename);
    static constexpr char kResourceDirname[] = "resources";

    void doInitCall() override {
        preloadResourceDirectory();
    }
    const CStringLifetime getInitCallName() const override { return "Preload resources"; }
   private:
    std::map<std::filesystem::path, std::string> kResources;
    std::vector<std::string> ignoredResources;
    std::once_flag m_once;
    static constexpr char kResourceLoadIgnoreFile[] = ".loadignore";
};
