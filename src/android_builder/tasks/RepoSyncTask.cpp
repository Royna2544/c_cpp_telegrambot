#include "RepoSyncTask.hpp"

#include <git2.h>

#include <algorithm>
#include <boost/algorithm/string/split.hpp>
#include <chrono>
#include <filesystem>
#include <functional>
#include <ios>
#include <regex>
#include <string>
#include <system_error>
#include <thread>
#include <utility>

#include "CompileTimeStringConcat.hpp"

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
    void add_diff_cleanup(git_diff* diff) {
        addCleanup([diff] { git_diff_free(diff); });
    }
    bool do_cleanup = true;

    RAIIGit() { git_libgit2_init(); }
    ~RAIIGit() {
        if (!do_cleanup) {
            return;
        }
        for (auto& cleanup : cleanups) {
            cleanup();
        }
        git_libgit2_shutdown();
    }
    RAIIGit(const RAIIGit&) = delete;
    RAIIGit(RAIIGit&& other) noexcept {
        std::swap(cleanups, other.cleanups);
        other.do_cleanup = false;
    }
};

struct MatchesData {
    std::filesystem::path gitDirectory;
    std::string desiredBranch;
    std::string desiredUrl;

   private:
    static constexpr std::string_view kRemoteRepoName = "origin";
    static const char* git_error_last_str() {
        return git_error_last()->message;
    }
    struct Callbacks {
        struct Data {
            git_repository* repo = nullptr;
            git_remote* remote = nullptr;
            git_reference* head_ref = nullptr;
            RAIIGit raii;
        };
        using fn_t = std::function<bool(Data&&)>;
        fn_t onMatchedCallback;
        fn_t onNoMatchCallback;
    };

    static bool hasDiff(git_diff* diff) {
        return git_diff_num_deltas(diff) != 0;
    }

    static void dumpDiff(git_diff* diff) {
        if (!hasDiff(diff)) {
            return;
        }
        size_t num_deltas = git_diff_num_deltas(diff);
        const git_diff_delta* delta = git_diff_get_delta(diff, 0);
        int i = 0;

        while (i < num_deltas) {
            delta = git_diff_get_delta(diff, i);
            git_diff_file file = delta->new_file;
            std::string statusString;
            switch (delta->status) {
                case GIT_DELTA_DELETED:
                    statusString = "added";
                    break;
                case GIT_DELTA_ADDED:
                    statusString = "deleted";
                    break;
                case GIT_DELTA_MODIFIED:
                    statusString = "modified";
                    break;
                case GIT_DELTA_RENAMED:
                    statusString = "renamed";
                    break;
                case GIT_DELTA_TYPECHANGE:
                    statusString = "typechange";
                    break;
                case GIT_DELTA_CONFLICTED:
                    statusString = "conflict";
                    break;
                default:
                    break;
            };
            if (!statusString.empty()) {
                statusString.insert(statusString.begin(), '(');
                statusString.insert(statusString.end(), ')');
                statusString.insert(statusString.begin(), ' ');
            }
            LOG(WARNING) << file.path << statusString;
            i++;
        }
    }

   public:
    [[nodiscard]] bool matchesAndCheckout() const {
        int ret = 0;
        git_reference* target_ref = nullptr;
        git_reference* target_remote_ref = nullptr;
        git_object* treeish = nullptr;
        git_commit* commit = nullptr;
        git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
        git_tree* head_tree = nullptr;
        git_tree* target_tree = nullptr;
        git_diff_options diffopts = GIT_DIFF_OPTIONS_INIT;
        git_diff* diff = nullptr;
        git_repository* repo = nullptr;
        git_reference* head_ref = nullptr;
        const char* current_branch = nullptr;
        const char* remote_url = nullptr;
        git_remote* remote = nullptr;
        RAIIGit raii;
        diffopts.flags = GIT_CHECKOUT_NOTIFY_CONFLICT;
        checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;

        ret = git_repository_open(&repo, gitDirectory.c_str());

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

        if (remote_url != desiredUrl) {
            LOG(INFO) << "Repository URL doesn't match...";
            return false;
        }

        ret = git_repository_head(&head_ref, repo);
        if (ret != 0) {
            LOG(ERROR) << "Failed to get HEAD reference: "
                       << git_error_last_str();
            return false;
        }
        raii.add_ref_cleanup(head_ref);

        current_branch = git_reference_shorthand(head_ref);

        Callbacks::Data data{repo, remote, head_ref, std::move(raii)};
        // Check if the branch is the one we're interested in
        if (desiredBranch == current_branch) {
            LOG(INFO) << "Already on the desired branch";
            return true;
        } else {
            LOG(INFO) << "Expected branch: " << desiredBranch
                      << ", current branch: " << current_branch;
        }
        LOG(INFO) << "Switching to branch: " << desiredBranch;
        struct {
            // refs/heads/*branch*
            std::string local;
            std::string remote;
        } target_ref_name;
        target_ref_name.local += "refs/heads/";
        target_ref_name.local += desiredBranch;
        target_ref_name.remote += "refs/remotes/";
        target_ref_name.remote += kRemoteRepoName.data();
        target_ref_name.remote += "/";
        target_ref_name.remote += desiredBranch;

        // Try to find the branch ref in the repository
        ret = git_reference_lookup(&target_ref, data.repo,
                                   target_ref_name.local.c_str());
        if (ret != 0) {
            LOG(WARNING) << "Failed to find the branch ref in local: "
                         << git_error_last_str();

            // Maybe remote has it?
            ret = git_remote_fetch(data.remote, nullptr, nullptr, nullptr);
            if (ret != 0) {
                LOG(ERROR) << "Failed to fetch remote: "
                           << git_error_last_str();
                return false;
            }

            // Now try to find the branch in the remote
            ret = git_reference_lookup(&target_remote_ref, data.repo,
                                       target_ref_name.remote.c_str());
            if (ret != 0) {
                LOG(ERROR) << "Failed to find the branch "
                              "in the remote: "
                           << git_error_last_str();
                return false;
            }
            data.raii.add_ref_cleanup(target_remote_ref);

            // Get the commit of the target branch ref
            ret = git_commit_lookup(&commit, data.repo,
                                    git_reference_target(target_remote_ref));
            if (ret != 0) {
                LOG(ERROR) << "Failed to find the commit: "
                           << git_error_last_str();
                return false;
            }
            data.raii.add_commit_cleanup(commit);

            // Create the local branch ref pointing to the commit
            ret = git_branch_create(&target_ref, data.repo,
                                    desiredBranch.c_str(), commit, 0);
            if (ret != 0) {
                LOG(ERROR) << "Failed to create branch: "
                           << git_error_last_str();
                return false;
            }

            ret = git_branch_set_upstream(target_ref,
                                          target_ref_name.remote.c_str());
            if (ret != 0) {
                // Not so fatal
                LOG(WARNING)
                    << "Failed to set upstream: " << git_error_last_str();
            }
            target_ref = target_remote_ref;
            LOG(INFO) << "Success on looking up remote branch";
        } else {
            // Switching to the branch directly
        }
        data.raii.add_ref_cleanup(target_ref);

        ret = git_commit_lookup(&commit, data.repo,
                                git_reference_target(target_ref));
        if (ret != 0) {
            LOG(ERROR) << "Failed to find the target ref commit: "
                       << git_error_last_str();
            return false;
        }
        data.raii.add_commit_cleanup(commit);

        // Create the tree object for the commit
        ret = git_commit_tree(&target_tree, commit);
        if (ret != 0) {
            LOG(ERROR) << "Failed to create target ref commit tree: "
                       << git_error_last_str();
            return false;
        }
        data.raii.add_tree_cleanup(target_tree);

        // Get the object of the target branch ref
        ret = git_reference_peel(&treeish, target_ref, GIT_OBJECT_TREE);
        if (ret != 0) {
            LOG(ERROR) << "Failed to find the branch: " << git_error_last_str();
            return false;
        }
        data.raii.add_object_cleanup(treeish);

        ret = git_diff_index_to_workdir(&diff, data.repo, nullptr, &diffopts);
        if (ret != 0) {
            LOG(ERROR) << "Failed to create unstaged diff: "
                       << git_error_last_str();
            return false;
        }
        data.raii.add_diff_cleanup(diff);

        if (hasDiff(diff)) {
            LOG(WARNING) << "You have unstaged changes";
            dumpDiff(diff);
            LOG(WARNING) << "Please commit your changes before you switch "
                            "branches. ";
            LOG(ERROR) << "Done.";
            return false;
        }
        ret = git_commit_lookup(&commit, data.repo,
                                git_reference_target(data.head_ref));
        if (ret != 0) {
            LOG(ERROR) << "Failed to find the head commit: "
                       << git_error_last_str();
            return false;
        }
        data.raii.add_commit_cleanup(commit);

        // Get the head tree of the repository
        ret = git_commit_tree(&head_tree, commit);
        if (ret != 0) {
            LOG(ERROR) << "Failed to get commit tree: " << git_error_last_str();
            return false;
        }
        data.raii.add_tree_cleanup(head_tree);

        ret = git_diff_tree_to_index(&diff, data.repo, head_tree, nullptr,
                                     &diffopts);
        if (ret != 0) {
            LOG(ERROR) << "Failed to create staged diff: "
                       << git_error_last_str();
            return false;
        }
        data.raii.add_diff_cleanup(diff);
        if (hasDiff(diff)) {
            LOG(WARNING) << "You have staged changes";
            dumpDiff(diff);
            LOG(WARNING) << "Please commit your changes before you switch "
                            "branches. ";
            LOG(ERROR) << "Done.";
            return false;
        }

        // Checkout the tree of the target branch ref to the repository
        ret = git_checkout_tree(data.repo, treeish, &checkout_opts);
        if (ret != 0) {
            LOG(ERROR) << "Failed to checkout tree: " << git_error_last_str();
            return false;
        }

        // Set the HEAD to the target branch ref
        ret = git_repository_set_head(data.repo, target_ref_name.local.c_str());
        if (ret != 0) {
            LOG(ERROR) << "Failed to set HEAD: " << git_error_last_str();
            return false;
        }
        LOG(INFO) << "Switched to branch: " << desiredBranch;
        return true;
    }

    [[nodiscard]] bool matches() {
        git_repository* repo = nullptr;
        int ret = 0;
        git_reference* head_ref = nullptr;
        git_reference* remote_tracking_ref = nullptr;
        const char* branchName = nullptr;
        const char* upstreamName = nullptr;
        RAIIGit raii;
        git_config* config = nullptr;

        ret =
            git_config_open_ondisk(&config, (gitDirectory / "config").c_str());
        if (ret != 0) {
            LOG(ERROR) << "Failed to open config file: "
                       << git_error_last_str();
            git_config_free(config);
            return false;
        }
        // This config may not exist... probably?
        if (ret == 0) {
            int val = 0;
            ret =
                git_config_get_bool(&val, config, "extensions.preciousobjects");
            if (ret == 0) {
                LOG(INFO) << "Removing extensions.preciousobjects...";
                ret = git_config_delete_entry(config,
                                              "extensions.preciousobjects");
                if (ret != 0) {
                    LOG(ERROR)
                        << "Failed to delete extensions.preciousobjects: "
                        << git_error_last_str();
                }
            } else {
                LOG(WARNING) << "Failed to get extensions.preciousobjects: "
                           << git_error_last_str();
            }
            // Manual, to save the file to disk
            git_config_free(config);
        }

        ret = git_repository_open(&repo, gitDirectory.c_str());
        if (ret != 0) {
            LOG(ERROR) << "Failed to open repository: " << git_error_last_str();
            return false;
        }
        raii.add_repo_cleanup(repo);
        ret = git_repository_head(&head_ref, repo);
        if (ret != 0) {
            LOG(ERROR) << "Failed to get head reference: "
                       << git_error_last_str();
            return false;
        }
        raii.add_ref_cleanup(head_ref);
        ret = git_branch_name(&branchName, head_ref);
        if (ret != 0) {
            LOG(ERROR) << "Failed to get branch name: " << git_error_last_str();
            return false;
        }
        ret = git_branch_upstream(&remote_tracking_ref, head_ref);
        if (ret == GIT_ENOTFOUND) {
            LOG(WARNING) << "Tracking remote is not configured???";
            return false;
        } else if (ret < 0) {
            LOG(ERROR) << "Failed to get upstream reference: "
                       << git_error_last_str();
            return false;
        } else {
            raii.add_ref_cleanup(remote_tracking_ref);
            ret = git_branch_name(&upstreamName, remote_tracking_ref);
            if (ret != 0) {
                LOG(ERROR) << "Failed to get upstream branch name: "
                           << git_error_last_str();
                return false;
            }
            // Exclude remote/ prefix
            std::string upstream_branch = upstreamName;
            constexpr auto originPrefix = StringConcat::cat("origin", "/");
            if (upstream_branch.starts_with(originPrefix)) {
                upstream_branch.erase(
                    0, originPrefix.getSize());  // Remove "remote/"
            }
            if (desiredBranch == upstream_branch) {
                LOG(INFO) << "Desired branch name matches upstream name";
                return true;
            } else {
                LOG(INFO) << "Local branch does not match remote, got "
                          << upstreamName;
                return false;
            }
        }
    }
};
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
    std::error_code ec;
    bool repoDirExists = false;

    repoDirExists = std::filesystem::is_directory(".repo/manifests.git", ec);
    if (ec &&
        ec != std::make_error_code(std::errc::no_such_file_or_directory)) {
        LOG(ERROR) << "Failed to check .repo directory existence: "
                   << ec.message();
        return false;
    } else {
        DLOG(INFO) << ".repo directory exists: " << std::boolalpha
                   << repoDirExists;
    }

    const auto& rom = getValue(data.localManifest->rom);
    try {
        RepoUtils utils;
        MatchesData mmdata{
            .gitDirectory = ".repo/manifests",
            .desiredBranch = rom->branch,
            .desiredUrl = rom->romInfo->url,
        };
        if (!repoDirExists || !mmdata.matches()) {
            utils.repo_init({rom->romInfo->url, rom->branch});
        }
        if (!std::filesystem::exists(kLocalManifestPath)) {
            utils.git_clone(data.localManifest->repo_info,
                            kLocalManifestPath.data());
        } else {
            LOG(INFO) << "Local manifest exists already...";
            MatchesData mdata{
                .gitDirectory = RepoSyncTask::kLocalManifestPath.data(),
                .desiredBranch = data.localManifest->repo_info.branch,
                .desiredUrl = data.localManifest->repo_info.url,
            };
            if (mdata.matchesAndCheckout()) {
                LOG(INFO) << "Repo is up-to-date.";
            } else {
                LOG(ERROR)
                    << "Repo sync not possible: local manifest is not mine";
                return false;
            }
        }
        unsigned int job_count = std::thread::hardware_concurrency() / 2;
        if (runWithReducedJobs) {
            // Futher reduce the number of jobs count
            utils.repo_sync(job_count / 4);
        } else {
            utils.repo_sync(job_count);
        }
    } catch (const std::exception& e) {
        LOG(ERROR) << "Error occurred during repo sync: " << e.what();
        return false;
    }
    return true;
}

void RepoSyncTask::onNewStderrBuffer(ForkAndRun::BufferType& buffer) {
    std::vector<std::string> lines;

    // Split the buffer into lines
    boost::split(lines, buffer.data(), [](const char c) { return c == '\n'; });
    if (std::chrono::system_clock::now() - clock > 10s) {
        clock = std::chrono::system_clock::now();
        wrapper->editMessage(message, "Sync in progress...:\n" + lines[0]);
    }
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
    }
}

void RepoSyncTask::onExit(int exitCode) {
    LOG(INFO) << "Repo sync exited with code: " << exitCode;
    auto* result = data.result;
    if (localHook.hasProblems()) {
        result->setMessage(localHook.getLogMessage());
        result->value = localHook.hasFatalProblems()
                            ? PerBuildData::Result::ERROR_FATAL
                            : PerBuildData::Result::ERROR_NONFATAL;
    } else if (networkHook.hasProblems()) {
        result->setMessage(networkHook.getLogMessage());
        result->value = PerBuildData::Result::ERROR_NONFATAL;
        // Will be picked up by next loop
        runWithReducedJobs = true;
    } else if (exitCode != 0) {
        result->value = PerBuildData::Result::ERROR_FATAL;
        result->setMessage("Repo sync failed");
    } else {
        result->value = PerBuildData::Result::SUCCESS;
        result->setMessage("Repo sync successful");
    }
    localHook.clearProblems();  // Per-sync only, retrying won't help
}

void RepoSyncTask::onSignal(int signalCode) {
    LOG(INFO) << "Repo sync received signal: " << strsignal(signalCode);
}

RepoSyncTask::RepoSyncTask(ApiPtr wrapper, Message::Ptr message,
                           PerBuildData data)
    : data(std::move(data)), wrapper(wrapper), message(std::move(message)) {}
