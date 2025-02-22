#include "RepoUtils.hpp"

#include <absl/log/log.h>
#include <absl/strings/str_replace.h>
#include <absl/strings/str_split.h>
#include <fmt/core.h>
#include <git2.h>

#include <filesystem>
#include <libos/libsighandler.hpp>
#include <memory>
#include <trivial_helpers/raii.hpp>
#include <type_traits>

#include "BytesConversion.hpp"

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
    operator T*() noexcept { return &raw_ptr_; }
    operator T() noexcept { return raw_ptr_; }
    bool operator==(std::nullptr_t /*null*/) const noexcept {
        return raw_ptr_ == nullptr;
    }
    bool operator!=(std::nullptr_t /*null*/) const noexcept {
        return !(*this == nullptr);
    }

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

struct GitBranchSwitcher::RepoInfoPriv {
    git_tree_ptr target_tree;
    git_remote_ptr remote;                 // Remote data of "origin" remote
    git_repository_ptr repo;               // Repository information
    git_reference_ptr head_ref;            // Current HEAD reference
    git_commit_ptr head_commit;            // The HEAD commit
    const char* remote_url = nullptr;      // URL of remote origin
    const char* current_branch = nullptr;  // Name of current branch
};

void GitBranchSwitcher::removeOffendingConfig() const {
    const auto configPath = _gitDirectory / "config";
    int ret;
    if (std::filesystem::exists(configPath)) {
        git_config* config = nullptr;
        DLOG(INFO) << "Config file found: " << configPath.string();
        ret = git_config_open_ondisk(&config, configPath.string().c_str());
        if (ret != 0) {
            LOG(WARNING) << "Failed to open config file: "
                         << git_error_last_str();
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
                        << "Failed to delete extensions.preciousobjects : "
                        << git_error_last_str();
                }
            }
            // Manual, to save the file to disk
            git_config_free(config);
        }
    }
}

bool GitBranchSwitcher::open() {
    int ret = 0;
    RepoInfoPriv data{};

    // Init libgit2
    git_libgit2_init();

    ret = git_repository_open(data.repo, _gitDirectory.string().c_str());

    if (ret != 0) {
        LOG(ERROR) << "Failed to open repository: " << git_error_last_str();
        return false;
    }

    ret = git_remote_lookup(data.remote, data.repo, kRemoteRepoName.data());
    if (ret != 0) {
        LOG(ERROR) << "Failed to lookup remote origin: "
                   << git_error_last_str();
        return false;
    }

    data.remote_url = git_remote_url(data.remote);
    if (data.remote_url == nullptr) {
        LOG(ERROR) << "Remote origin URL is null: " << git_error_last_str();
        return false;
    }
    LOG(INFO) << "Remote origin URL: " << data.remote_url;

    ret = git_repository_head(data.head_ref, data.repo);
    if (ret != 0) {
        LOG(ERROR) << "Failed to get HEAD reference: " << git_error_last_str();
        return false;
    }

    ret = git_commit_lookup(data.head_commit, data.repo,
                            git_reference_target(data.head_ref));
    if (ret != 0) {
        LOG(ERROR) << "Failed to find the head commit: "
                   << git_error_last_str();
        return true;
    }

    data.current_branch = git_reference_shorthand(data.head_ref);

    // Success
    rdata = std::make_unique<RepoInfoPriv>(std::move(data));
    cdata = nullptr;
    return true;
}

struct GitBranchSwitcher::CheckoutInfoPriv {
    struct {  // refs/heads/ *branch*
        std::string local;
        std::string remote;
    } target_ref_name;
    git_reference_ptr target_local_ref;   // The target reference in local
    git_reference_ptr target_remote_ref;  // The target reference in remote
    bool fetched = true;
    git_diff_options diff_opts = GIT_DIFF_OPTIONS_INIT;
    git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
    const char* remote_refname() const {
        return target_ref_name.remote.c_str();
    }

    const char* local_refname() const { return target_ref_name.local.c_str(); }

    static std::string make_local_refname(const std::string_view branch) {
        return fmt::format("refs/heads/{}", branch);
    }

    CheckoutInfoPriv(git_repository* repo, git_remote* remote,
                     std::string_view destBranch) {
        diff_opts.flags = GIT_CHECKOUT_NOTIFY_CONFLICT;
        checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;
        // Craft strings
        target_ref_name.local = make_local_refname(destBranch);
        target_ref_name.remote =
            fmt::format("refs/remotes/{}/{}", kRemoteRepoName, destBranch);

        // Now try to find the branch in local first
        int ret = git_reference_lookup(target_local_ref, repo, local_refname());
        if (ret != 0) {
            LOG(WARNING) << "Didn't find local ref: " << git_error_last_str();
        }

        // If local didn't exist, maybe remote has it?
        // First update the repo data with fetch.
        ret = git_remote_fetch(remote, nullptr, nullptr, nullptr);
        if (ret != 0) {
            LOG(WARNING) << "Failed to fetch remote: " << git_error_last_str();
            // This is only for updating origin/ refs, not a requirement.
            fetched = false;
        }

        // Now try to find the branch in the remote
        ret = git_reference_lookup(target_remote_ref, repo, remote_refname());
        if (ret != 0) {
            LOG(WARNING) << "Didn't find remote ref: " << git_error_last_str();
        }
    }
};

bool GitBranchSwitcher::fastForwardPull() const {
    git_merge_options merge_opts = GIT_MERGE_OPTIONS_INIT;
    git_annotated_commit_ptr remote_commit;
    git_merge_analysis_t analysis;
    git_merge_preference_t preference;
    int ret;

    if (!rdata || !cdata || cdata->target_local_ref == nullptr ||
        cdata->target_remote_ref == nullptr) {
        LOG(ERROR) << "Invalid data";
        return false;
    }

    // Get the remote branch's OID (SHA)
    ret = git_annotated_commit_from_ref(remote_commit, rdata->repo,
                                        cdata->target_remote_ref);
    if (ret != 0) {
        LOG(WARNING) << "Failed to get remote commit: " << git_error_last_str();
        return false;
    }

    const git_annotated_commit* remote_commits[] = {remote_commit};
    if (git_merge_analysis(&analysis, &preference, rdata->repo, remote_commits,
                           1) != 0) {
        LOG(WARNING) << "Failed to analyze merge: " << git_error_last_str();
        return false;
    }

    if (!(analysis & GIT_MERGE_ANALYSIS_FASTFORWARD)) {
        LOG(INFO) << "FastForward not possible";
        return false;
    }

    return checkout(cdata->remote_refname());
}

bool GitBranchSwitcher::isRefnameSame(
    git_repository* repo,
    const std::pair<std::string_view, std::string_view>& refnames) {
    std::pair<git_oid, git_oid> oids{};
    if (git_reference_name_to_id(&oids.first, repo, refnames.first.data()) !=
        0) {
        LOG(ERROR) << "Failed to resolve " << refnames.first << ": "
                   << git_error_last_str();
        return false;
    }
    if (git_reference_name_to_id(&oids.second, repo, refnames.second.data()) !=
        0) {
        LOG(ERROR) << "Failed to resolve " << refnames.second << ": "
                   << git_error_last_str();
        return false;
    }
    bool isSame = git_oid_cmp(&oids.first, &oids.second) == 0;
    DLOG(INFO) << fmt::format("{} and {} are {}the same", refnames.first,
                              refnames.second, isSame ? "" : "not ");
    return isSame;
}

bool GitBranchSwitcher::hasUnstagedChanges() const {
    int ret;
    git_diff_ptr diff;
    git_tree_ptr head_tree;

    if (!rdata) {
        LOG(ERROR) << "Missing RepoDataPriv";
        return true;
    }

    if (!cdata) {
        LOG(ERROR) << "Missing CheckoutDataPriv";
        return true;
    }

    ret = git_diff_index_to_workdir(diff, rdata->repo, nullptr,
                                    &cdata->diff_opts);
    if (ret != 0) {
        LOG(ERROR) << "Failed to create unstaged diff: "
                   << git_error_last_str();
        return true;
    }
    if (hasDiff(diff)) {
        LOG(WARNING) << "You have unstaged changes";
        dumpDiff(diff);
        LOG(WARNING)
            << "Please commit your changes before you switch branches. ";
        LOG(ERROR) << "Done.";
        return true;
    }

    // Get the head tree of the repository
    ret = git_commit_tree(head_tree, rdata->head_commit);
    if (ret != 0) {
        LOG(ERROR) << "Failed to get commit tree: " << git_error_last_str();
        return true;
    }
    ret = git_diff_tree_to_index(diff, rdata->repo, head_tree, nullptr,
                                 &cdata->diff_opts);
    if (ret != 0) {
        LOG(ERROR) << "Failed to create staged diff: " << git_error_last_str();
        return true;
    }
    if (hasDiff(diff)) {
        LOG(WARNING) << "You have staged changes";
        dumpDiff(diff);
        LOG(WARNING) << "Please commit your changes before you switch "
                        "branches. ";
        LOG(ERROR) << "Done.";
        return true;
    }
    return false;
}

bool GitBranchSwitcher::check(const RepoInfo& info) const {
    if (!rdata) {
        DLOG(INFO) << "No rdata";
        return false;
    }
    if (info.url() != rdata->remote_url) {
        DLOG(INFO) << "Mismatch on URL";
        return false;
    }

    if (rdata->current_branch != info.branch()) {
        DLOG(INFO) << "Branch mismatch";

        return isRefnameSame(
            rdata->repo,
            std::make_pair<>("HEAD", CheckoutInfoPriv::make_local_refname(

                                         info.branch())));
    }
    return true;
}

bool GitBranchSwitcher::checkout(const std::string_view refname) const {
    git_oid target_oid;
    git_object_ptr target_commit;
    int ret;

    // Get the target commit's OID
    ret = git_reference_name_to_id(&target_oid, rdata->repo, refname.data());
    if (ret != 0) {
        LOG(WARNING) << "Failed to resolve remote reference: "
                     << git_error_last_str();
        return false;
    }

    // Lookup the target commit
    ret = git_object_lookup(static_cast<git_object**>(target_commit),
                            rdata->repo, &target_oid, GIT_OBJECT_COMMIT);
    if (ret != 0) {
        LOG(WARNING) << "Failed to lookup target commit: "
                     << git_error_last_str();
        return false;
    }

    ret = git_checkout_tree(rdata->repo, target_commit, &cdata->checkout_opts);
    if (ret != 0) {
        LOG(WARNING) << "Failed to checkout target commit: "
                     << git_error_last_str();
        return false;
    }

    // Update HEAD to the new commit
    ret = git_repository_set_head(rdata->repo, refname.data());

    if (ret != 0) {
        LOG(WARNING) << "Failed to update HEAD: " << git_error_last_str();
        return false;
    }

    ret = git_checkout_head(rdata->repo, &cdata->checkout_opts);

    if (ret != 0) {
        LOG(WARNING) << "Failed to checkout HEAD: " << git_error_last_str();
        return false;
    }
    return true;
}

bool GitBranchSwitcher::checkout(const RepoInfo& info) {
    int ret;

    if (!rdata) {
        LOG(ERROR) << "No RepoInfoPriv";
        return false;
    }
    if (info.url() != rdata->remote_url) {
        LOG(WARNING) << "URL mismatch";
        return false;
    }

    if (info.branch() == rdata->current_branch) {
        LOG(INFO) << "Already on " << info.branch();
        return true;
    }

    cdata = std::make_unique<CheckoutInfoPriv>(rdata->repo, rdata->remote,
                                               info.branch());

    // Check if local or remote has the needed ref
    if (cdata->target_local_ref == nullptr &&
        cdata->target_remote_ref == nullptr) {
        LOG(WARNING) << "Didn't find branch named " << info.branch()
                     << " on either remote and local";
        return false;
    }

    if (hasUnstagedChanges()) {
        return false;
    }
    LOG(INFO) << "Switching to branch: " << info.branch();

    // Try to find the branch ref in the repository
    if (cdata->target_local_ref == nullptr) {
        git_commit_ptr remote_head_commit;
        // Get the commit of the target branch ref
        ret = git_commit_lookup(remote_head_commit, rdata->repo,
                                git_reference_target(cdata->target_remote_ref));
        if (ret != 0) {
            LOG(ERROR) << "Failed to find the commit: " << git_error_last_str();
            return false;
        }

        // Create the local branch ref pointing to the commit
        ret = git_branch_create(cdata->target_local_ref, rdata->repo,
                                info.branch().c_str(), remote_head_commit, 0);
        if (ret != 0) {
            LOG(ERROR) << "Failed to create branch: " << git_error_last_str();
            return false;
        }
        ret = git_branch_set_upstream(cdata->target_local_ref,
                                      cdata->remote_refname());
        if (ret != 0) {  // Not so fatal
            LOG(WARNING) << "Failed to set upstream: " << git_error_last_str();
        }
        LOG(INFO) << "Success on looking up remote branch";
    } else {
        // Switching to the branch directly
        fastForwardPull();
    }

    // Checkout the branch
    if (!checkout(cdata->local_refname())) {
        LOG(WARNING) << "Failed to checkout branch";
        return false;
    }
    LOG(INFO) << "Switched to branch: " << info.branch();

    // Update current branch metadata
    ret = git_repository_head(rdata->head_ref, rdata->repo);
    if (ret != 0) {
        LOG(ERROR) << "Failed to update HEAD reference: "
                   << git_error_last_str();
        return false;
    }
    rdata->current_branch = git_reference_shorthand(rdata->head_ref);
    return true;
}

GitBranchSwitcher::GitBranchSwitcher(std::filesystem::path gitDirectory)
    : _gitDirectory(std::move(gitDirectory)) {}

GitBranchSwitcher::~GitBranchSwitcher() {
    rdata.reset();
    cdata.reset();
    git_libgit2_shutdown();
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
        +[](const git_indexer_progress* stats, void* payload) -> int {
        LOG_EVERY_N_SEC(INFO, 3) << fmt::format(
            "Fetch: Objects({}/{}/{}) Deltas({}/{}), Total "
            "{:.2f}GB",
            stats->received_objects, stats->indexed_objects,
            stats->total_objects, stats->indexed_deltas, stats->total_deltas,
            GigaBytes(stats->received_bytes * boost::units::data::bytes)
                .value());
        if (payload != nullptr) {
            auto* callback = static_cast<Callbacks*>(payload);
            callback->onFetch(stats);
        }
        return -static_cast<int>(
            SignalHandler::isSignaled());  // Continue the transfer.
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
        return -static_cast<int>(
            SignalHandler::isSignaled());  // Continue the transfer.
    };
    gitoptions.fetch_opts.callbacks.payload = callback_.get();

    // Git checkout callback
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
#ifdef LIBGIT2_HAS_CLONE_DEPTH
        gitoptions.fetch_opts.depth = 1;
#else
#warning "Shallow clone not supported"
        LOG(WARNING) << "Shallow cloning is not supported";
#endif
    }

    git_libgit2_init();
    int ret = ::git_clone(repo, url_.c_str(), directory.string().c_str(), &gitoptions);
    if (ret != 0) {
        const auto* fault = git_error_last();
        LOG(ERROR) << "Couldn't clone repo: " << fault->message;
    } else {
        LOG(INFO) << "Git repository cloned successfully";
    }
    git_libgit2_shutdown();
    return ret == 0;
}
