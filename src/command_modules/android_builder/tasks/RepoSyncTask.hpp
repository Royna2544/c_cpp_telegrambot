#include <ConfigParsers.hpp>
#include <ForkAndRun.hpp>
#include <RepoUtils.hpp>
#include <api/TgBotApi.hpp>
#include <chrono>

class NewStdErrBufferHook {
    std::stringstream logMessage;

   protected:
    bool hadProblems = false;
    bool hadFatalProblems = false;

    void errorAndLog(const std::string& message) {
        LOG(ERROR) << message;
        logMessage << message << std::endl;
    }

   public:
    /**
     * @brief Virtual method to process each line of stderr buffer.
     *
     * This method is intended to be overridden by subclasses to handle specific
     * processing logic for stderr lines.
     *
     * @param line A single line of text from the stderr buffer.
     * @return A boolean value indicating whether the hook processed the line.
     *
     * @note This method is called for each line of stderr output during the
     * execution of the task.
     *
     * @note Subclasses should override this method to implement their own
     * processing logic.
     */
    virtual bool process(const std::string& line) = 0;

    [[nodiscard]] std::string getLogMessage() const noexcept {
        return logMessage.str();
    }
    [[nodiscard]] bool hasProblems() const noexcept { return hadProblems; }
    [[nodiscard]] bool hasFatalProblems() const noexcept {
        return hadProblems && hadFatalProblems;
    }
    void clearProblems() noexcept {
        hadProblems = false;
        hadFatalProblems = false;
        logMessage.str("");
    }
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
    bool hasRemoveIssues = false;

   public:
    bool process(const std::string& line) override;
    ~RepoSyncLocalHook() override = default;
};

class RepoSyncNetworkHook : public NewStdErrBufferHook {
   public:
    bool process(const std::string& line) override;
    ~RepoSyncNetworkHook() override = default;
};

struct RepoSyncTask : ForkAndRun {
    constexpr static std::string_view kLocalManifestPath =
        ".repo/local_manifests";

    DeferredExit runFunction() override;

    /**
     * @brief Handles new standard error data.
     *
     * This function is called when new standard error data is available. It
     * overrides the base class's onNewStderrBuffer() method to provide custom
     * behavior.
     *
     * @param buffer The buffer containing the new standard error data.
     */
    void onNewStderrBuffer(ForkAndRun::BufferType& buffer) override;

    /**
     * @brief Handles the process exit event.
     *
     * This function is called when the process exits. It overrides the base
     * class's onExit() method to provide custom behavior.
     *
     * @param exitCode The exit code of the process.
     */
    void onExit(int exitCode) override;

    /**
     * @brief Handles the process signal event.
     *
     * This function is called when the process receives a signal. It overrides
     * the base class's onSignal() method to provide custom behavior.
     *
     * @param signalCode The signal code that the process received.
     */
    void onSignal(int signalCode) override;

    /**
     * Creates a new RepoSyncTask object.
     *
     * @param api A pointer to the Telegram Bot API wrapper.
     * @param message The Telegram message associated with the build task.
     * @param data Per-build configuration and path data.
     */
    explicit RepoSyncTask(TgBotApi::CPtr api, Message::Ptr message,
                          PerBuildData data);

   private:
    PerBuildData data;
    RepoSyncLocalHook localHook;
    RepoSyncNetworkHook networkHook;
    TgBotApi::CPtr api;
    Message::Ptr message;
    std::chrono::system_clock::time_point clock =
        std::chrono::system_clock::now();
    bool runWithReducedJobs = false;
};