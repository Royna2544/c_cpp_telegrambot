#include <tgbot/types/InputTextMessageContent.h>

#include <ConfigParsers.hpp>
#include <ForkAndRun.hpp>
#include <api/TgBotApi.hpp>
#include <chrono>
#include <memory>
#include <string_view>

#include "../../support/KeyBoardBuilder.hpp"
#include "Shmem.hpp"

struct ROMBuildTask : ForkAndRun {
    static constexpr std::string_view kShmemROMBuild = "shmem_rombuild";
    static constexpr std::string_view kErrorLogFile = "out/error.log";
    static constexpr std::string_view kPreLogFile = "out/preappend.log";

    struct RBEConfig {
        std::filesystem::path baseScript;
        std::string api_key;
        std::filesystem::path reclientDir;
    };

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

    /**
     * @brief Handles new standard output data.
     *
     * This function is called when new standard output data is available. It
     * overrides the base class's onNewStdoutBuffer() method to provide custom
     * behavior.
     *
     * @param buffer The buffer containing the new standard output data.
     */
    void handleStdoutData(ForkAndRun::BufferViewType buffer) override;

    /**
     * @brief Handles new standard output data.
     *
     * This function is called when new standard output data is available. It
     * overrides the base class's onNewStdoutBuffer() method to provide custom
     * behavior.
     *
     * @param buffer The buffer containing the new standard output data.
     */
    void handleStderrData(ForkAndRun::BufferViewType buffer) override;

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
     * Creates a new ROMBuildTask object.
     *
     * @param api A pointer to the Telegram Bot API wrapper.
     * @param message The Telegram message associated with the build task.
     * @param data Per-build configuration and path data.
     */
    explicit ROMBuildTask(TgBotApi::Ptr api, Message::Ptr message,
                          PerBuildData data, std::optional<RBEConfig> rbecfg);
    ~ROMBuildTask() override;

   private:
    PerBuildData data;
    TgBot::Message::Ptr message;
    TgBotApi::Ptr api;
    std::unique_ptr<AllocatedShmem> smem;
    std::chrono::system_clock::time_point clock;
    std::chrono::system_clock::time_point startTime;
    TgBot::InputTextMessageContent::Ptr textContent;
    KeyboardBuilder builder;
    std::optional<RBEConfig> _rbeConfig;
    bool once = false;
};
