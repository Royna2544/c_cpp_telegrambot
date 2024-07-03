#include <ConfigParsers.hpp>
#include <ForkAndRun.hpp>
#include <RepoUtils.hpp>
#include "PerBuildData.hpp"

struct RepoSyncTask : ForkAndRun {
    constexpr static std::string_view kLocalManifestPath =
        ".repo/local_manifests";

    /**
     * @brief Runs the function that performs the repository synchronization.
     *
     * This function is responsible for executing the repository synchronization
     * process. It overrides the base class's runFunction() method to provide
     * custom behavior.
     *
     * @return True if the synchronization process is successful, false
     * otherwise.
     */
    bool runFunction() override;

    /**
     * @brief Handles new standard output data.
     *
     * This function is called when new standard output data is available. It
     * overrides the base class's onNewStdoutBuffer() method to provide custom
     * behavior.
     *
     * @param buffer The buffer containing the new standard output data.
     */
    void onNewStdoutBuffer(ForkAndRun::BufferType& buffer) override;

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
     * @brief Constructs a RepoSyncF object with the provided data.
     *
     * This constructor initializes a RepoSyncF object with the given data.
     *
     * @param data The data object containing the necessary configuration and
     * paths.
     */
    explicit RepoSyncTask(PerBuildData data);

   private:
    PerBuildData data;
    std::atomic_bool networkSyncError = false;
};