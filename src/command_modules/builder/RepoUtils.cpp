#include "RepoUtils.hpp"

#include <absl/log/log.h>
#include <absl/strings/str_replace.h>
#include <absl/strings/str_split.h>
#include <fmt/core.h>
#include <git2.h>
#include <git2/pack.h>
#include <trivial_helpers/_class_helper_macros.h>

#include <filesystem>
#include <memory>
#include <trivial_helpers/raii.hpp>
#include <type_traits>

#include "SystemInfo.hpp"

struct ScopedLibGit2 {
    ScopedLibGit2() { git_libgit2_init(); }
    ~ScopedLibGit2() { git_libgit2_shutdown(); }
    NO_MOVE_CTOR(ScopedLibGit2);
    NO_COPY_CTOR(ScopedLibGit2);
};

template <typename T>
struct deleter {
    void operator()(T* ptr) const = delete;
};

template <typename T>
concept hasDeleter = requires(T* t) { deleter<T>()(t); };

template <>
struct deleter<git_repository> {
    void operator()(git_repository* repo) const { git_repository_free(repo); }
};

template <>
struct deleter<git_reference> {
    void operator()(git_reference* ref) const { git_reference_free(ref); }
};

template <>
struct deleter<git_commit> {
    void operator()(git_commit* commit) const { git_commit_free(commit); }
};

template <>
struct deleter<git_tree> {
    void operator()(git_tree* tree) const { git_tree_free(tree); }
};

template <>
struct deleter<git_object> {
    void operator()(git_object* obj) const { git_object_free(obj); }
};

template <>
struct deleter<git_remote> {
    void operator()(git_remote* remote) const { git_remote_free(remote); }
};

template <>
struct deleter<git_diff> {
    void operator()(git_diff* diff) const { git_diff_free(diff); }
};

template <>
struct deleter<git_annotated_commit> {
    void operator()(git_annotated_commit* commit) const {
        git_annotated_commit_free(commit);
    }
};

template <hasDeleter T>
inline auto git_wrap(T* object) {
    return RAII<T*>::template create<void>(object, [](T* obj) {
        if (obj) {
            deleter<T>()(obj);
        }
    });
}

template <typename T>
    requires std::is_pointer_v<T>
struct GitPtrWrapper {
    using value_type = std::remove_pointer_t<T>;

    GitPtrWrapper() : _ptr(git_wrap<value_type>(nullptr)) {}
    GitPtrWrapper(T ptr)
        : _ptr(git_wrap<value_type>(ptr)), raw_ptr_(ptr.get()) {}

    // aka, value_type**
    operator T*() { return &raw_ptr_; }
    operator bool() const { return _ptr; }
    operator T() { return raw_ptr_; }

   private:
    T raw_ptr_ = nullptr;
    std::unique_ptr<value_type, typename RAII<T>::template Deleter<void>> _ptr;
};

using git_repository_ptr = GitPtrWrapper<git_repository*>;
using git_reference_ptr = GitPtrWrapper<git_reference*>;
using git_commit_ptr = GitPtrWrapper<git_commit*>;
using git_annotated_commit_ptr = GitPtrWrapper<git_annotated_commit*>;
using git_tree_ptr = GitPtrWrapper<git_tree*>;
using git_object_ptr = GitPtrWrapper<git_object*>;
using git_remote_ptr = GitPtrWrapper<git_remote*>;
using git_diff_ptr = GitPtrWrapper<git_diff*>;

const char* GitBranchSwitcher::git_error_last_str() {
    return git_error_last()->message;
}
bool GitBranchSwitcher::hasDiff(git_diff* diff) {
    return git_diff_num_deltas(diff) != 0;
}

void GitBranchSwitcher::dumpDiff(git_diff* diff) {
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

bool GitBranchSwitcher::operator()() const {
    int ret = 0;
    git_reference_ptr target_ref;
    git_reference_ptr target_remote_ref;
    git_object_ptr treeish;
    git_commit_ptr commit;
    git_tree_ptr head_tree;
    git_tree_ptr target_tree;
    git_diff_ptr diff;
    git_remote_ptr remote;
    git_repository_ptr repo;
    git_reference_ptr head_ref;
    const char* current_branch = nullptr;
    ScopedLibGit2 _;

    git_diff_options diffopts = GIT_DIFF_OPTIONS_INIT;
    diffopts.flags = GIT_CHECKOUT_NOTIFY_CONFLICT;
    git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
    checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;

    // We could either get the repo, or the .git directory
    const auto configPath = gitDirectory / "config";
    if (std::filesystem::exists(configPath)) {
        git_config* config = nullptr;
        DLOG(INFO) << "Config file found: " << configPath.string();
        ret = git_config_open_ondisk(&config, configPath.c_str());
        if (ret != 0) {
            LOG(WARNING) << "Failed to open config file: "
                         << git_error_last_str();
            git_config_free(config);
        } else {
            int val = 0;
            ret =
                git_config_get_bool(&val, config, "extensions.preciousObjects");
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
    }

    ret = git_repository_open(repo, gitDirectory.c_str());

    if (ret != 0) {
        LOG(ERROR) << "Failed to open repository: " << git_error_last_str();
        return false;
    }

    ret = git_remote_lookup(remote, repo, kRemoteRepoName.data());
    if (ret != 0) {
        LOG(ERROR) << "Failed to lookup remote origin: "
                   << git_error_last_str();
        return false;
    }

    const char* remote_url = git_remote_url(remote);
    if (remote_url == nullptr) {
        LOG(ERROR) << "Remote origin URL is null: " << git_error_last_str();
        return false;
    }
    LOG(INFO) << "Remote origin URL: " << remote_url;

    if (remote_url != desiredUrl) {
        LOG(INFO) << "Repository URL doesn't match...";
        return false;
    }

    ret = git_repository_head(head_ref, repo);
    if (ret != 0) {
        LOG(ERROR) << "Failed to get HEAD reference: " << git_error_last_str();
        return false;
    }

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

    bool fetchOk = true;
    // Maybe remote has it?
    ret = git_remote_fetch(remote, nullptr, nullptr, nullptr);
    if (ret != 0) {
        LOG(WARNING) << "Failed to fetch remote: " << git_error_last_str();
        // This is only for updating origin/ refs, not a requirement.
        fetchOk = false;
    }

    // Now try to find the branch in the remote
    ret = git_reference_lookup(target_remote_ref, repo,
                               target_ref_name.remote.c_str());
    if (ret != 0) {
        LOG(ERROR) << "Failed to find the branch "
                      "in the remote: "
                   << git_error_last_str();
        return false;
    }

    // Optional: Try ff
    do {
        git_merge_options merge_opts = GIT_MERGE_OPTIONS_INIT;
        // Get the remote branch's OID (SHA)
        git_annotated_commit_ptr remote_commit;

        ret = git_annotated_commit_from_ref(remote_commit, repo,
                                            target_remote_ref);
        if (ret != 0) {
            LOG(WARNING) << "Failed to get remote commit: "
                         << git_error_last_str();
            break;
        }
        git_annotated_commit** _commit = remote_commit;
        ret = git_merge(repo, const_cast<const git_annotated_commit**>(_commit),
                        1, &merge_opts, &checkout_opts);
        if (ret != 0) {
            LOG(WARNING) << "Failed to merge branch: " << git_error_last_str();
            break;
        }
    } while (false);

    current_branch = git_reference_shorthand(head_ref);
    // Check if the branch is the one we're interested in
    if (desiredBranch == current_branch) {
        LOG(INFO) << "Already on the desired branch";
        return true;
    } else {
        LOG(INFO) << "Expected branch: " << desiredBranch
                  << ", current branch: " << current_branch;
        if (!checkout) {
            // You know, we may be on the same sha1 still, check it.
            git_oid local_head_oid{};
            git_oid remote_head_oid{};
            if (git_reference_name_to_id(&local_head_oid, repo, "HEAD") != 0) {
                LOG(ERROR) << "Failed to resolve local HEAD: "
                           << git_error_last_str();
                return false;
            }
            if (git_reference_name_to_id(&remote_head_oid, repo,
                                         target_ref_name.remote.c_str()) != 0) {
                LOG(ERROR) << "Failed to resolve remote HEAD: "
                           << git_error_last_str();
                return false;
            }
            if (git_oid_cmp(&local_head_oid, &remote_head_oid) != 0) {
                LOG(INFO) << "Checkout: false. Done";
                return false;
            } else {
                LOG(INFO) << "Local and remote HEAD are the same";
                return true;
            }
        }
    }
    LOG(INFO) << "Switching to branch: " << desiredBranch;

    // Try to find the branch ref in the repository
    ret = git_reference_lookup(target_ref, repo, target_ref_name.local.c_str());
    if (ret != 0) {
        LOG(WARNING) << "Failed to find the branch ref in local: "
                     << git_error_last_str();

        // Get the commit of the target branch ref
        ret = git_commit_lookup(commit, repo,
                                git_reference_target(target_remote_ref));
        if (ret != 0) {
            LOG(ERROR) << "Failed to find the commit: " << git_error_last_str();
            return false;
        }

        // Create the local branch ref pointing to the commit
        ret = git_branch_create(target_ref, repo, desiredBranch.c_str(), commit,
                                0);
        if (ret != 0) {
            LOG(ERROR) << "Failed to create branch: " << git_error_last_str();
            return false;
        }

        ret =
            git_branch_set_upstream(target_ref, target_ref_name.remote.c_str());
        if (ret != 0) {
            // Not so fatal
            LOG(WARNING) << "Failed to set upstream: " << git_error_last_str();
        }
        target_ref = std::move(target_remote_ref);
        LOG(INFO) << "Success on looking up remote branch";
    } else {
        // Switching to the branch directly
    }

    ret = git_commit_lookup(commit, repo, git_reference_target(target_ref));
    if (ret != 0) {
        LOG(ERROR) << "Failed to find the target ref commit: "
                   << git_error_last_str();
        return false;
    }

    // Create the tree object for the commit
    ret = git_commit_tree(target_tree, commit);
    if (ret != 0) {
        LOG(ERROR) << "Failed to create target ref commit tree: "
                   << git_error_last_str();
        return false;
    }

    // Get the object of the target branch ref
    ret = git_reference_peel(treeish, target_ref, GIT_OBJECT_TREE);
    if (ret != 0) {
        LOG(ERROR) << "Failed to find the branch: " << git_error_last_str();
        return false;
    }

    ret = git_diff_index_to_workdir(diff, repo, nullptr, &diffopts);
    if (ret != 0) {
        LOG(ERROR) << "Failed to create unstaged diff: "
                   << git_error_last_str();
        return false;
    }

    if (hasDiff(diff)) {
        LOG(WARNING) << "You have unstaged changes";
        dumpDiff(diff);
        LOG(WARNING) << "Please commit your changes before you switch "
                        "branches. ";
        LOG(ERROR) << "Done.";
        return false;
    }
    ret = git_commit_lookup(commit, repo, git_reference_target(head_ref));
    if (ret != 0) {
        LOG(ERROR) << "Failed to find the head commit: "
                   << git_error_last_str();
        return false;
    }

    // Get the head tree of the repository
    ret = git_commit_tree(head_tree, commit);
    if (ret != 0) {
        LOG(ERROR) << "Failed to get commit tree: " << git_error_last_str();
        return false;
    }

    ret = git_diff_tree_to_index(diff, repo, head_tree, nullptr, &diffopts);
    if (ret != 0) {
        LOG(ERROR) << "Failed to create staged diff: " << git_error_last_str();
        return false;
    }
    if (hasDiff(diff)) {
        LOG(WARNING) << "You have staged changes";
        dumpDiff(diff);
        LOG(WARNING) << "Please commit your changes before you switch "
                        "branches. ";
        LOG(ERROR) << "Done.";
        return false;
    }

    // Checkout the tree of the target branch ref to the repository
    ret = git_checkout_tree(repo, treeish, &checkout_opts);
    if (ret != 0) {
        LOG(ERROR) << "Failed to checkout tree: " << git_error_last_str();
        return false;
    }

    // Set the HEAD to the target branch ref
    ret = git_repository_set_head(repo, target_ref_name.local.c_str());
    if (ret != 0) {
        LOG(ERROR) << "Failed to set HEAD: " << git_error_last_str();
        return false;
    }
    LOG(INFO) << "Switched to branch: " << desiredBranch;
    return true;
}

bool RepoInfo::git_clone(const std::filesystem::path& directory,
                         bool shallow) const {
    git_repository_ptr repo;
    git_clone_options gitoptions = GIT_CLONE_OPTIONS_INIT;

    LOG(INFO) << fmt::format(
        "Cloning repository, url: {}, branch: {}, shallow: {}", url_, branch_,
        shallow);
    gitoptions.checkout_branch = branch_.c_str();

    // Git fetch callback
    gitoptions.fetch_opts.callbacks.transfer_progress =
        +[](const git_transfer_progress* stats, void* payload) -> int {
        LOG_EVERY_N_SEC(INFO, 3) << fmt::format(
            "Fetch: Objects({}/{}/{}) Deltas({}/{}), Total {:.2f}GB",
            stats->received_objects, stats->indexed_objects,
            stats->total_objects, stats->indexed_deltas, stats->total_deltas,
            Bytes(stats->received_bytes).to<SizeTypes::GigaBytes>().value);
        if (payload != nullptr) {
            auto* callback = static_cast<Callbacks*>(payload);
            callback->onFetch(stats);
        }
        return 0;  // Continue the transfer.
    };
    gitoptions.fetch_opts.callbacks.payload = callback_.get();

    // Update refs callback
    gitoptions.fetch_opts.callbacks.pack_progress =
        +[](int stage, uint32_t current, uint32_t total, void* payload) -> int {
        LOG_EVERY_N_SEC(INFO, 3)
            << fmt::format("Packing: ({}/{})", current, total);

        if (payload != nullptr) {
            auto* callback = static_cast<Callbacks*>(payload);
            callback->onPacking(stage, current, total);
        }
        return 0;  // Continue the transfer.
    };
    gitoptions.fetch_opts.callbacks.payload = callback_.get();

    // Git checkout callbackd3866 
    gitoptions.checkout_opts.progress_cb =
        +[](const char* path, size_t completed_steps, size_t total_steps,
            void* payload) {
            LOG_EVERY_N_SEC(INFO, 3) << fmt::format(
                "Packing: ({}/{}): {}", completed_steps, total_steps,
                (path != nullptr) ? path : "(null)");
            if (payload != nullptr) {
                auto* callback = static_cast<Callbacks*>(payload);
                callback->onCheckout(path, completed_steps, total_steps);
            }
        };
    gitoptions.checkout_opts.progress_payload = callback_.get();

    if (shallow) {
#if LIBGIT2_VER_MAJOR > 0 || (LIBGIT2_VER_MAJOR == 0 && LIBGIT2_VER_MINOR >= 24)
        gitoptions.fetch_opts.depth = 1;
#else
        LOG(WARNING) << "Shallow cloning is not supported in libgit2 version < 0.24";
#endif
    }

    ScopedLibGit2 _;
    int ret = ::git_clone(repo, url_.c_str(), directory.c_str(), &gitoptions);
    if (ret != 0) {
        const auto* fault = git_error_last();
        LOG(ERROR) << "Couldn't clone repo: " << fault->message;
    } else {
        LOG(INFO) << "Git repository cloned successfully";
    }
    return ret == 0;
}
