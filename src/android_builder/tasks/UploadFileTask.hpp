#include <absl/log/log.h>

#include <TgBotWrapper.hpp>
#include <cstdlib>
#include <ConfigParsers.hpp>

#include "ForkAndRun.hpp"

struct UploadFileTask : ForkAndRun {
    static constexpr std::string_view kShmemUpload = "shmem_upload";

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
     */
    explicit UploadFileTask(PerBuildData data);

    ~UploadFileTask() override;
   private:
    PerBuildData data;
    Shmem smem;
};