#include "RepoSyncTask.hpp"

#include <git2.h>

#include <filesystem>
#include <functional>
#include <iostream>
#include <regex>
#include <stdexcept>
#include <string>
#include <thread>

#include "CStringLifetime.h"
#include "git2/types.h"
#include "tasks/PerBuildData.hpp"

namespace {

struct RAIIGit {
    std::vector<std::function<void(void)>> cleanups;
    void addCleanup(std::function<void(void)> cleanupFn) {
        cleanups.emplace_back(cleanupFn);
    }
    void add_repo_cleanup(git_repository* repo) {
        addCleanup([repo] { git_repository_free(repo); });
    }
    void add_ref_cleanup(git_reference* ref) {
        addCleanup([ref] { git_reference_free(ref); });
    }
    void add_commit_cleanup(git_commit* commit) {
        addCleanup([commit] { git_commit_free(commit); });
    }
    void add_tree_cleanup(git_tree* tree) {
        addCleanup([tree] { git_tree_free(tree); });
    }
    void add_object_cleanup(git_object* obj) {
        addCleanup([obj] { git_object_free(obj); });
    }
    void add_remote_cleanup(git_remote* remote) {
        addCleanup([remote] { git_remote_free(remote); });
    }

    RAIIGit() { git_libgit2_init(); }
    ~RAIIGit() {
        for (auto& cleanup : cleanups) {
            cleanup();
        }
        git_libgit2_shutdown();
    }
};

// Determine if the repository contains same url as the data, and try to match
// it
bool tryToMakeItMine(const PerBuildData& data) {
    RAIIGit raii;
    const auto git_error_last_str = [] { return git_error_last()->message; };
    int ret = 0;
    constexpr std::string_view kRemoteRepoName = "origin";

    git_repository* repo = nullptr;
    git_reference* head_ref = nullptr;
    git_reference* target_ref = nullptr;
    git_reference* target_remote_ref = nullptr;
    const char* current_branch = nullptr;
    const char* remote_url = nullptr;
    git_object* treeish = nullptr;
    git_remote* remote = nullptr;
    git_commit* commit = nullptr;
    git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;

    ret = git_repository_open(&repo, RepoSyncTask::kLocalManifestPath.data());
    if (ret != 0) {
        LOG(ERROR) << "Failed to open repository: " << git_error_last_str();
        return false;
    }
    raii.add_repo_cleanup(repo);

    ret = git_remote_lookup(&remote, repo, kRemoteRepoName.data());
    if (ret != 0) {
        LOG(ERROR) << "Failed to lookup remote origin: "
                   << git_error_last_str();
        return false;
    }
    raii.add_remote_cleanup(remote);

    remote_url = git_remote_url(remote);
    if (remote_url == nullptr) {
        LOG(ERROR) << "Remote origin URL is null: " << git_error_last_str();
        return false;
    }
    LOG(INFO) << "Remote origin URL: " << remote_url;

    if (remote_url != data.bConfig.local_manifest.url) {
        LOG(INFO) << "Repository URL doesn't match, ignoring";
        return false;
    }

    ret = git_repository_head(&head_ref, repo);
    if (ret != 0) {
        LOG(ERROR) << "Failed to get HEAD reference: " << git_error_last_str();
        return false;
    }
    raii.add_ref_cleanup(head_ref);

    current_branch = git_reference_shorthand(head_ref);
    LOG(INFO) << "Current branch: " << current_branch;
    CStringLifetime branch_name = data.bConfig.local_manifest.branch;

    // Check if the branch is the one we're interested in
    if (data.bConfig.local_manifest.branch != current_branch) {
        LOG(INFO) << "Switching to branch: " << branch_name.get();
        struct {
            // refs/heads/*branch*
            std::string local;
            std::string remote;
        } target_ref_name;
        target_ref_name.local += "refs/heads/";
        target_ref_name.local += branch_name.get();
        target_ref_name.remote += "refs/remotes/";
        target_ref_name.remote += kRemoteRepoName.data();
        target_ref_name.remote += "/";
        target_ref_name.remote += branch_name.get();

        // Try to find the branch ref in the repository
        ret = git_reference_lookup(&target_ref, repo,
                                   target_ref_name.local.c_str());
        if (ret != 0) {
            LOG(ERROR) << "Failed to find the branch ref: "
                       << git_error_last_str();

            // Maybe remote has it?
            ret = git_remote_fetch(remote, nullptr, nullptr, nullptr);
            if (ret != 0) {
                LOG(ERROR) << "Failed to fetch remote: "
                           << git_error_last_str();
                return false;
            }

            // Now try to find the branch in the remote
            ret = git_reference_lookup(&target_remote_ref, repo,
                                       target_ref_name.remote.c_str());
            if (ret != 0) {
                LOG(ERROR) << "Failed to find the branch "
                              "in the remote: "
                           << git_error_last_str();
                return false;
            }
            raii.add_ref_cleanup(target_remote_ref);

            // Get the commit of the target branch ref
            ret = git_commit_lookup(&commit, repo,
                                    git_reference_target(target_remote_ref));
            if (ret != 0) {
                LOG(ERROR) << "Failed to find the commit: "
                           << git_error_last_str();
                return false;
            }
            raii.add_commit_cleanup(commit);

            // Create the local branch ref pointing to the commit
            ret = git_branch_create(&target_ref, repo, branch_name.get(),
                                    commit, 0);
            if (ret != 0) {
                LOG(ERROR) << "Failed to create branch: "
                           << git_error_last_str();
                return false;
            }
            raii.add_ref_cleanup(target_ref);

            // Checkout the local branch
            ret = git_checkout_head(repo, &checkout_opts);
            if (ret != 0) {
                LOG(ERROR) << "Failed to checkout head: "
                           << git_error_last_str();
                return false;
            }

            LOG(INFO) << "Success on checking out remote branch";
        } else {
            // Switching to the branch directly
            raii.add_ref_cleanup(target_ref);

            // Get the object of the target branch ref
            ret = git_reference_peel(&treeish, target_ref, GIT_OBJECT_TREE);
            if (ret != 0) {
                LOG(ERROR) << "Failed to find the branch: "
                           << git_error_last_str();
                return false;
            }
            raii.add_object_cleanup(treeish);

            ret = git_checkout_tree(repo, treeish, nullptr);
            if (ret != 0) {
                LOG(ERROR) << "Failed to checkout tree: "
                           << git_error_last_str();
                return false;
            }
        }
        ret = git_repository_set_head(repo, target_ref_name.local.c_str());
        if (ret != 0) {
            LOG(ERROR) << "Failed to set HEAD: " << git_error_last_str();
            return false;
        }
    } else {
        LOG(INFO) << "Already on the desired branch";
    }
    return true;
}
}  // namespace

bool RepoSyncLocalHook::process(const std::string& line) {
    static const std::regex kRepoRemoveFail(
        "error: ([a-zA-Z0-9_\\-\\/]+): Cannot remove project: "
        "uncommitted changes are present\\.");
    std::smatch smatch;

    if (line.find(kUpdatingFiles) != std::string::npos) {
        // Stop logspam
        return true;
    }
    if (line.find(kChangesWouldBeLost) != std::string::npos) {
        errorAndLog(
            "Repo sync failed due to local issue: Changes would be lost");
        hadProblems = true;
        return true;
    }

    if (line.find(kRepoCheckoutFailStart) != std::string::npos) {
        errorAndLog("Repo sync failed due to local issue: CheckoutFail");
        hasCheckoutIssues = true;
        hadProblems = true;
        return true;
    }

    if (std::regex_search(line, smatch, kRepoRemoveFail)) {
        std::error_code ec;
        errorAndLog("Repo sync failed due to local issue: RemoveFail");
        hasRemoveIssues = true;
        hadProblems = true;
        if (smatch.size() > 1) {
            std::string directory = smatch[1].str();
            LOG(WARNING) << "Will remove directory: " << directory;
            std::filesystem::remove_all(directory, ec);
            if (ec) {
                errorAndLog("Failed to remove: " + directory + ": " +
                            ec.message());
                hadFatalProblems = true;
            } else {
                LOG(INFO) << "Successfully removed";
            }
        }
    }

    if (hasCheckoutIssues) {
        std::error_code ec;
        if (line.find(kRepoCheckoutFailEnd) != std::string::npos) {
            // Clear the flag
            hasCheckoutIssues = false;
            return true;
        }
        std::filesystem::remove_all(line, ec);
        std::string msg = "Checkout Failed Directory: " + line + ": ";
        if (ec) {
            errorAndLog(msg + "Failed to remove: " + ec.message());
            hadFatalProblems = true;
        } else {
            errorAndLog(msg + "Removed");
        }
        return true;
    }
    return false;
}
bool RepoSyncNetworkHook::process(const std::string& line) {
    static const std::regex kSyncErrorNetworkRegex(
        R"(^error:\s+Cannot\s+fetch\s+[^\s]+(?:/[^\s]+)*\s+from\s+https:\/\/.+$)");
    static const std::regex kSyncErrorNetworkRegex2(
        R"(^Failed to connect to (github\.com|gitlab\.com) port \d+ after \d+ ms: Couldn't connect to server$)");
    if (std::regex_match(line, kSyncErrorNetworkRegex) ||
        std::regex_match(line, kSyncErrorNetworkRegex2)) {
        LOG(INFO) << "Detected sync issue, caused by network";
        hadProblems = true;
        return true;
    }
    return false;
}

bool RepoSyncTask::runFunction() {
    try {
        RepoUtils utils(data.scriptDirectory);
        utils.repo_init({
            .url = data.rConfig.url,
            .branch = data.rConfig.branch,
        });
        if (!std::filesystem::exists(kLocalManifestPath)) {
            utils.git_clone(data.bConfig.local_manifest,
                            kLocalManifestPath.data());
        } else {
            LOG(INFO) << "Local manifest exists already...";
            if (tryToMakeItMine(data)) {
                LOG(INFO) << "Repo is up-to-date.";
            } else {
                LOG(ERROR)
                    << "Repo sync not possible: local manifest is not mine";
                return false;
            }
        }
        if (networkHook.hasProblems()) {
            utils.repo_sync(std::thread::hardware_concurrency() / 4);
        } else {
            utils.repo_sync(std::thread::hardware_concurrency());
        }
    } catch (const std::exception& e) {
        LOG(ERROR) << "Error occurred during repo sync: " << e.what();
        return false;
    }
    return true;
}

void RepoSyncTask::onNewStderrBuffer(ForkAndRun::BufferType& buffer) {
    std::vector<std::string> lines;

    boost::split(lines, buffer.data(), [](const char c) { return c == '\n'; });
    for (const auto& line : lines) {
        if (line.empty()) {
            continue;
        }
        if (localHook.process(line)) {
            continue;
        }
        if (networkHook.process(line)) {
            continue;
        }
        // std::cerr << "Repo sync stderr: " << line << std::endl;
    }
}

void RepoSyncTask::onExit(int exitCode) {
    LOG(INFO) << "Repo sync exited with code: " << exitCode;
    if (localHook.hasProblems()) {
        data.result->setMessage(localHook.getLogMessage());
        data.result->value = localHook.hasFatalProblems()
                                 ? PerBuildData::Result::ERROR_FATAL
                                 : PerBuildData::Result::ERROR_NONFATAL;
    } else if (networkHook.hasProblems()) {
        data.result->setMessage(networkHook.getLogMessage());
        data.result->value = PerBuildData::Result::ERROR_NONFATAL;
    } else if (exitCode != 0) {
        data.result->value = PerBuildData::Result::ERROR_FATAL;
        data.result->setMessage("Repo sync failed");
    } else {
        data.result->value = PerBuildData::Result::SUCCESS;
        data.result->setMessage("Repo sync successful");
    }
    localHook.clearProblems(); // Per-sync only, retrying won't help
}

void RepoSyncTask::onSignal(int signalCode) {
    LOG(INFO) << "Repo sync received signal: " << strsignal(signalCode);
}

RepoSyncTask::RepoSyncTask(PerBuildData data) : data(std::move(data)) {}
