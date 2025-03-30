#include <absl/log/log.h>

#include <ConfigParsers.hpp>
#include <cstdlib>

#include "ForkAndRun.hpp"
#include "Shmem.hpp"

struct UploadFileTask : ForkAndRun {
    static constexpr std::string_view kShmemUploadArtifact = "shmem_upload_artifact";

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
    DeferredExit runFunction() override;

    void handleStdoutData(ForkAndRun::BufferViewType buffer) override;
    void handleStderrData(ForkAndRun::BufferViewType buffer) override;

    /**
     * @brief Callback function for handling the exit of the subprocess.
     *
     * This method is called when the subprocess exits.
     *
     * @param exitCode The exit code of the subprocess.
     */
    void onExit(int exitCode) override;

    /**
     * @brief Constructs a UploadFileTask object with the provided data.
     *
     * This constructor initializes a RepoSyncF object with the given data.
     *
     * @param data The data object containing the necessary configuration and
     * paths.
     * @param scriptsDirectory The directory containing the script files.
     */
    explicit UploadFileTask(PerBuildData data,
                            std::filesystem::path scriptsDirectory);

    ~UploadFileTask() override;

   private:
    PerBuildData data{};
    std::unique_ptr<AllocatedShmem> artifact_smem;
    std::string outputString;
    std::mutex stdout_mutex;
    std::filesystem::path _scriptDirectory;
};
