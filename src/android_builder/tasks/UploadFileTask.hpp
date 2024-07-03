#include <absl/log/log.h>

#include <MessageWrapper.hpp>
#include <cstdlib>
#include <iostream>
#include <utility>

#include "ArgumentBuilder.hpp"
#include "ForkAndRun.hpp"
#include "PythonClass.hpp"
#include "tasks/PerBuildData.hpp"

struct UploadFileTask : ForkAndRun {
    ~UploadFileTask() override = default;

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
    bool runFunction() override {
        auto py = PythonClass::get();
        py->addLookupDirectory(data.scriptDirectory);
        auto mod = py->importModule("upload_file");
        if (!mod) {
            LOG(ERROR) << "Could not import module upload_file";
            return false;
        }
        // Device name, PrefixStr
        auto func = mod->lookupFunction("upload_to_gofile");
        if (!func) {
            LOG(ERROR) << "Could not find function upload_to_gofile";
            return false;
        }
        ArgumentBuilder builder(2);
        builder.add_argument(data.bConfig.device);
        builder.add_argument(data.rConfig.prefixOfOutput);
        std::string resultString;
        if (!func->call(builder.build(), &resultString)) {
            LOG(ERROR) << "Error calling function upload_to_gofile";
            return false;
        }
        LOG(INFO) << resultString;
        wrapper.sendMessageOnExit(resultString);
        return true;
    }

    /**
     * @brief Callback function for handling the exit of the subprocess.
     *
     * This method is called when the subprocess exits.
     *
     * @param exitCode The exit code of the subprocess.
     */
    void onExit(int exitCode) override {
        const auto& bot = wrapper.getBot();
        switch (exitCode) {
            case EXIT_SUCCESS:
                bot_sendMessage(bot, wrapper.getChatId(),
                                "Upload completed successfully");

                break;
            case EXIT_FAILURE:
            default:
                bot_sendMessage(bot, wrapper.getChatId(), "Upload failed");
        }
    }

    /**
     * @brief Constructs a UploadFileTask object with the provided data.
     *
     * This constructor initializes a RepoSyncF object with the given data.
     *
     * @param data The data object containing the necessary configuration and
     * paths.
     */
    explicit UploadFileTask(MessageWrapper& wrapper, PerBuildData data)
        : wrapper(wrapper), data(std::move(data)) {}

   private:
    MessageWrapper&
        wrapper;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    PerBuildData data;
};