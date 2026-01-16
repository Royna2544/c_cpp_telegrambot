#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

// constexpr-qualifable struct
struct ConstRepoInfo {
    std::string_view url;
    std::string_view branch;
};

class RepoInfo {
    std::string url_;
    std::string branch_;

   public:
    /**
     * @brief Virtual callback methods for tracking progress during repository
     * operations.
     *
     * These methods are intended to be overridden in derived classes to provide
     * custom behavior.
     */
    class ProgressNotifier {
       public:
        virtual ~ProgressNotifier() = default;

        /**
         * This structure is used to provide callers information about the
         * progress of indexing a packfile, either directly or part of a
         * fetch or clone that downloads a packfile.
         */
        struct TransferStats {
            /** number of objects in the packfile being indexed */
            unsigned int total_objects;

            /** received objects that have been hashed */
            unsigned int indexed_objects;

            /** received_objects: objects which have been downloaded */
            unsigned int received_objects;

            /** number of deltas in the packfile being indexed */
            unsigned int total_deltas;

            /** received deltas that have been indexed */
            unsigned int indexed_deltas;

            /** size of the packfile received up to now */
            size_t received_bytes;
        };

        enum class PackBuilderStage : std::uint8_t {
            UNKNOWN,
            ADDING_OBJECTS,
            DELTAFICATION
        };

        /**
         * @brief Called during fetch operations to provide progress
         * information.
         *
         * @param stats A pointer to a `TransferStats` structure
         * containing fetch statistics.
         */
        virtual void onFetch(const TransferStats* stats) {}

        /**
         * @brief Called during indexing operations to provide progress
         * information.
         *
         * @param stage The current indexing stage. (PackBuilderStage::*)
         * @param current The current indexing progress.
         * @param total The total indexing progress.
         */
        virtual void onPacking(PackBuilderStage stage, uint32_t current,
                               uint32_t total) {}

        /**
         * @brief Called during checkout operations to provide progress
         * information.
         *
         * @param path The path of the file or directory being checked out.
         * @param completed_steps The number of completed checkout steps.
         * @param total_steps The total number of checkout steps.
         */
        virtual void onCheckout(const char* path, size_t completed_steps,
                                size_t total_steps) {}
    };

    RepoInfo(std::string url, std::string branch)
        : url_(std::move(url)), branch_(std::move(branch)) {}
    explicit RepoInfo(const ConstRepoInfo info)
        : url_(info.url), branch_(info.branch) {}
    RepoInfo() = default;

    template <typename Callback>
    RepoInfo() : callback_(std::make_unique<Callback>()) {}

    void setCallback(std::unique_ptr<ProgressNotifier> callback) {
        callback_ = std::move(callback);
    }

    // Git clone repo to a directory
    bool git_clone(const std::filesystem::path& directory,
                   const std::optional<std::string_view> gitToken,
                   bool shallow = false) const;

    [[nodiscard]] std::string url() const { return url_; }
    [[nodiscard]] std::string branch() const { return branch_; }

   private:
    std::unique_ptr<ProgressNotifier> callback_;
};

class GitBranchSwitcher {
    std::filesystem::path _gitDirectory;

    static constexpr std::string_view kRemoteRepoName = "origin";

    // Remove extensions.preciousobjects which is
    // used on Google .repo git and is
    // unsupported on libgit2
    void removeOffendingConfig() const;
    // Checkout to refname. Incl. HEAD, tree, index.
    bool checkout(const std::string_view refname) const;

    struct RepoInfoPriv;
    std::unique_ptr<RepoInfoPriv> rdata;
    struct CheckoutInfoPriv;
    std::unique_ptr<CheckoutInfoPriv> cdata;

   public:
    // Create a Git branch switcher with given Git Directory
    explicit GitBranchSwitcher(std::filesystem::path gitDirectory);
    ~GitBranchSwitcher();

    // Open the git repo if possible.
    bool open();
    // Compare the current repo info against RepoInfo
    [[nodiscard]] bool check(const RepoInfo& info) const;
    // Actually checkout to the RepoInfo info, and return result
    bool checkout(const RepoInfo& info);
    // Check if the working tree has any unstaged things
    [[nodiscard]] bool hasUnstagedChanges() const;
    // Try a FF-pull.
    [[nodiscard]] bool fastForwardPull() const;
};
