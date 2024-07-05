#include "RepoSyncTask.hpp"

#include <filesystem>
#include <iostream>
#include <regex>
#include <string>
#include <thread>

#include "tasks/PerBuildData.hpp"

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
            LOG(INFO) << "Local manifest exists, skipping";
        }
        utils.repo_sync(std::thread::hardware_concurrency());
        // If network sync error occurs, the flag is immediately set but this
        // repo sync will end later, so it's enough to make it atomic bool.
        if (networkSyncError) {
            // Let's hope no one is building in dual core cpu.
            utils.repo_sync(std::thread::hardware_concurrency() / 4);
        }
    } catch (const std::exception& e) {
        LOG(ERROR) << "Error occurred during repo sync: " << e.what();
        return false;
    }
    return true;
}

void RepoSyncTask::onNewStdoutBuffer(ForkAndRun::BufferType& buffer) {
    std::vector<std::string> lines;
    boost::split(lines, buffer.data(), [](const char c) { return c == '\n'; });
    for (const auto& line : lines) {
        if (line.empty()) {
            continue;
        }
        std::cout << "Repo sync stdout: " << line << std::endl;
    }
}

class NewStdErrBufferHook {
   public:
    enum Action { kContinueLoop, kPass, kErrorLocalIssue, kErrorNetworkIssue };

    /**
     * @brief Virtual method to process each line of stderr buffer.
     *
     * This method is intended to be overridden by subclasses to handle specific
     * processing logic for stderr lines.
     *
     * @param line A single line of text from the stderr buffer.
     * @return An enum value indicating whether the loop should continue or
     * pass.
     *
     * @note This method is called for each line of stderr output during the
     * execution of the task.
     *
     * @note Subclasses should override this method to implement their own
     * processing logic.
     */
    virtual Action process(const std::string& line) = 0;

    virtual ~NewStdErrBufferHook() = default;
};

class RepoSyncLocalHook : public NewStdErrBufferHook {
    static constexpr std::string_view kUpdatingFiles = "Updating files:";
    static constexpr std::string_view kChangesWouldBeLost =
        "error: Your local changes to the following files would be overwritten "
        "by checkout:";
    static constexpr std::string_view kRepoCheckoutFailStart = "Failing repos:";
    static constexpr std::string_view kRepoCheckoutFailEnd =
        "Try re-running with \"-j1 --fail-fast\" to exit at the first error.";
    bool hasCheckoutIssues = false;

   public:
    Action process(const std::string& line) override {
        if (line.find(kUpdatingFiles) != std::string::npos) {
            // Stop logspam
            return Action::kContinueLoop;
        }
        if (line.find(kChangesWouldBeLost) != std::string::npos) {
            LOG(ERROR)
                << "Repo sync failed due to local issue: Changes would be lost";
            return Action::kErrorLocalIssue;
        }

        if (line.find(kRepoCheckoutFailStart) != std::string::npos) {
            LOG(ERROR) << "Repo sync failed due to local issue: CheckoutFail";
            hasCheckoutIssues = true;
            return Action::kErrorLocalIssue;
        }

        if (hasCheckoutIssues) {
            std::error_code ec;
            if (line.find(kRepoCheckoutFailEnd) != std::string::npos) {
                // Clear the flag
                hasCheckoutIssues = false;
                return Action::kErrorLocalIssue;
            }
            LOG(INFO) << "Repo sync failed directory: " << line;
            std::filesystem::remove_all(line, ec);
            if (ec) {
                LOG(ERROR) << "Failed to remove directory: " << line
                           << ", error: " << ec.message();
            }
            return Action::kErrorLocalIssue;
        }
        return Action::kPass;
    }
    ~RepoSyncLocalHook() override = default;
};

class RepoSyncNetworkHook : public NewStdErrBufferHook {
   public:
    Action process(const std::string& line) override {
        static const std::regex kSyncErrorNetworkRegex(
            R"(^error:\s+Cannot\s+fetch\s+[^\s]+(?:/[^\s]+)*\s+from\s+https:\/\/.+$)");
        if (std::regex_match(line, kSyncErrorNetworkRegex)) {
            LOG(INFO) << "Detected sync issue, caused by network";
            return Action::kErrorNetworkIssue;
        }
        return Action::kPass;
    }
    ~RepoSyncNetworkHook() override = default;
};

void RepoSyncTask::onNewStderrBuffer(ForkAndRun::BufferType& buffer) {
    std::vector<std::string> lines;
    RepoSyncLocalHook localHook;
    RepoSyncNetworkHook networkHook;

    boost::split(lines, buffer.data(), [](const char c) { return c == '\n'; });
    for (const auto& line : lines) {
        if (line.empty()) {
            continue;
        }
        switch (localHook.process(line)) {
            case NewStdErrBufferHook::kContinueLoop:
                continue;
            case NewStdErrBufferHook::kPass:
                break;
            case NewStdErrBufferHook::kErrorLocalIssue:
            case NewStdErrBufferHook::kErrorNetworkIssue:
                // Nothing for now...
                break;
        }
        switch (networkHook.process(line)) {
            case NewStdErrBufferHook::kContinueLoop:
                continue;
            case NewStdErrBufferHook::kPass:
                break;
            case NewStdErrBufferHook::kErrorNetworkIssue:
                networkSyncError = true;
                break;
            default:
                break;
        }
        std::cerr << "Repo sync stderr: " << line << std::endl;
    }
}

void RepoSyncTask::onExit(int exitCode) {
    LOG(INFO) << "Repo sync exited with code: " << exitCode;
    *data.result = exitCode == 0;
}
void RepoSyncTask::onSignal(int signalCode) {
    LOG(INFO) << "Repo sync received signal: " << strsignal(signalCode);
}

RepoSyncTask::RepoSyncTask(PerBuildData data) : data(std::move(data)) {}
