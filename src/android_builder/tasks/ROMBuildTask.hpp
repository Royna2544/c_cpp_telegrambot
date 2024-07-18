#include <ForkAndRun.hpp>
#include <TgBotWrapper.hpp>
#include <chrono>
#include <memory>
#include <string_view>

#include "PerBuildData.hpp"
#include "PythonClass.hpp"

struct ROMBuildTask : ForkAndRun {
    static constexpr std::string_view kShmemROMBuild = "shmem_rombuild";
    static constexpr std::string_view kErrorLogFile = "out/error.log";

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

    int guessJobCount();
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
     * @brief Handles the process exit event.
     *
     * This function is called when the process exits. It overrides the base
     * class's onExit() method to provide custom behavior.
     *
     * @param exitCode The exit code of the process.
     */
    void onExit(int exitCode) override;

    [[noreturn]] static void errorAndThrow(const std::string& message);

    /**
     * @brief Constructs a ROMBuildTask object with the provided data.
     *
     * This constructor initializes a RepoSyncF object with the given data.
     *
     * @param data The data object containing the necessary configuration and
     * paths.
     */
    explicit ROMBuildTask(ApiPtr wrapper, TgBot::Message::Ptr message,
                          PerBuildData data);
    ~ROMBuildTask() override;

   private:
    PerBuildData data;
    TgBot::Message::Ptr message;
    std::shared_ptr<TgBotApi> botWrapper;
    PythonClass::Ptr _py;
    PythonClass::FunctionHandle::Ptr _get_total_mem;
    PythonClass::FunctionHandle::Ptr _get_used_mem;
    Shmem smem{};
    decltype(std::chrono::system_clock::now()) clock;
    decltype(std::chrono::system_clock::now()) startTime;
};