#pragma once

#include <filesystem>
#include <memory>
#include <string>

#include "PythonClass.hpp"

// Calls scripts/repo_utils.py
// Wrapper for the python script
class RepoUtils {
   public:
    struct RepoInfo {
        std::string url;
        std::string branch;
    };
    // We will just throw an runtime_exception if they fail.
    void repo_init(const RepoInfo& options);
    void repo_sync(const long job_count);
    static void git_clone(const RepoInfo& options, const std::filesystem::path& directory);
    explicit RepoUtils();
    ~RepoUtils();

   private:
    std::shared_ptr<PythonClass::FunctionHandle> reposync_function;
    std::shared_ptr<PythonClass::FunctionHandle> repoinit_function;
};