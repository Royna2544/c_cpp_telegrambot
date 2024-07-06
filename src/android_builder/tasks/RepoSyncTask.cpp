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
#include "tasks/PerBuildData.hpp"

namespace {

struct RAIIGit {
    std::vector<std::function<void(void)>> cleanups;
    void addCleanup(std::function<void(void)> cleanupFn) {
        cleanups.emplace_back(cleanupFn);
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

    git_repository* repo = nullptr;
    git_reference* head_ref = nullptr;
    const char* current_branch = nullptr;
    git_object* treeish = nullptr;
    git_remote* remote = nullptr;
    const char* remote_url = nullptr;

    ret = git_repository_open(&repo, RepoSyncTask::kLocalManifestPath.data());
    if (ret != 0) {
        LOG(ERROR) << "Failed to open repository";
        return false;
    }
    raii.addCleanup([repo] { git_repository_free(repo); });

    ret = git_remote_lookup(&remote, repo, "origin");
    if (ret != 0) {
        LOG(ERROR) << "Failed to lookup remote origin";
        return false;
    }
    raii.addCleanup([remote] { git_remote_free(remote); });

    remote_url = git_remote_url(remote);
    if (remote_url == nullptr) {
        LOG(ERROR) << "Remote origin URL is null";
        return false;
    }
    LOG(INFO) << "Remote origin URL: " << remote_url;

    if (remote_url != data.bConfig.local_manifest.url) {
        LOG(INFO) << "Repository URL doesn't match, ignoring";
        return false;
    }

    ret = git_repository_head(&head_ref, repo);
    if (ret != 0) {
        LOG(ERROR) << "Failed to get HEAD reference";
        return false;
    }
    raii.addCleanup([head_ref] { git_reference_free(head_ref); });

    current_branch = git_reference_shorthand(head_ref);
    LOG(INFO) << "Current branch: " << current_branch;
    CStringLifetime branch_name = data.bConfig.local_manifest.branch;

    if (data.bConfig.local_manifest.branch != current_branch) {
        LOG(INFO) << "Switching to branch: "
                  << data.bConfig.local_manifest.branch;

        ret = git_revparse_single(&treeish, repo, branch_name);
        if (ret != 0) {
            LOG(ERROR) << "Failed to find the branch";
            return false;
        }
        raii.addCleanup([treeish] { git_object_free(treeish); });

        ret = git_checkout_tree(repo, treeish, nullptr);
        if (ret != 0) {
            LOG(ERROR) << "Failed to checkout tree";
            return false;
        }
        ret = git_repository_set_head(
            repo, ("refs/heads/" + std::string(branch_name)).c_str());
        if (ret != 0) {
            LOG(ERROR) << "Failed to set HEAD";
            return false;
        }
    } else {
        LOG(INFO) << "Already on the desired branch";
    }
    return true;
}
}  // namespace

bool RepoSyncLocalHook::process(const std::string& line) {
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
    if (std::regex_match(line, kSyncErrorNetworkRegex)) {
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
        try {
            utils.repo_sync(std::thread::hardware_concurrency());
        } catch (const std::runtime_error& e) {
            // Try with lower job count
            utils.repo_sync(std::thread::hardware_concurrency() / 4);
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
        std::cerr << "Repo sync stderr: " << line << std::endl;
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
    localHook.clearProblems();
    networkHook.clearProblems();
}

void RepoSyncTask::onSignal(int signalCode) {
    LOG(INFO) << "Repo sync received signal: " << strsignal(signalCode);
}

RepoSyncTask::RepoSyncTask(PerBuildData data) : data(std::move(data)) {}
