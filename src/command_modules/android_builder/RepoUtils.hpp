#pragma once

#include <git2/diff.h>

#include <filesystem>
#include <string>
#include "ForkAndRun.hpp"

struct RepoInfo {
    std::string url;
    std::string branch;
};

class RepoUtils {
   public:
    using RepoInfo = ::RepoInfo;

    [[nodiscard]] static DeferredExit repo_init(const RepoInfo& options);
    [[nodiscard]] static DeferredExit repo_sync(const long job_count);
};

class GitUtils {
   public:
    using RepoInfo = ::RepoInfo;

    static bool git_clone(const RepoInfo& options,
                          const std::filesystem::path& directory);
};

struct GitBranchSwitcher {
    std::filesystem::path gitDirectory;
    std::string desiredBranch;
    std::string desiredUrl;
    bool checkout = false;

   private:
    static constexpr std::string_view kRemoteRepoName = "origin";
    static const char* git_error_last_str();
    static bool hasDiff(git_diff* diff);
    static void dumpDiff(git_diff* diff);

   public:
    [[nodiscard]] bool operator()() const;
};