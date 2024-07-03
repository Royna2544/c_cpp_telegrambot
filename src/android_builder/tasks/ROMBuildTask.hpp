#include <BotClassBase.h>
#include <absl/log/log.h>
#include <math.h>
#include <sys/sysinfo.h>

#include <ArgumentBuilder.hpp>
#include <ConfigParsers.hpp>
#include <ForkAndRun.hpp>
#include <RepoUtils.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <chrono>
#include <iomanip>
#include <memory>
#include <mutex>
#include <ostream>
#include <thread>
#include <utility>

#include "BotReplyMessage.h"
#include "PerBuildData.hpp"
#include "PythonClass.hpp"
#include "internal/_std_chrono_templates.h"

struct ROMBuildTask : ForkAndRun, BotClassBase {
    ~ROMBuildTask() override = default;

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
        auto repomod = py->importModule("build_rom_utils");
        if (!repomod) {
            LOG(ERROR) << "Failed to import build_rom_utils module";
            return false;
        }
        auto build_rom = repomod->lookupFunction("build_rom");
        if (!build_rom) {
            LOG(ERROR) << "Failed to find build_rom function";
            return false;
        }
        ArgumentBuilder builder(4);
        builder.add_argument(data.bConfig.device);
        switch (data.bConfig.variant) {
            case BuildConfig::Variant::kUser:
                builder.add_argument("user");
                break;
            case BuildConfig::Variant::kUserDebug:
                builder.add_argument("userdebug");
                break;
            case BuildConfig::Variant::kEng:
                builder.add_argument("eng");
                break;
        }
        builder.add_argument(data.rConfig.target);
        builder.add_argument(guessJobCount());
        bool result = false;
        if (!build_rom->call<bool>(builder.build(), &result)) {
            LOG(ERROR) << "Failed to call build ROM";
        }
        if (result) {
            LOG(INFO) << "ROM build succeeded";
        } else {
            LOG(ERROR) << "ROM build failed";
        }
        *data.result = result;
        return result;
    }

    int guessJobCount() {
        static std::once_flag once;
        static int jobCount = 6;
        std::call_once(once, [this] {
            double total_memory = NAN;
            if (!_get_total_mem->call<double>(nullptr, &total_memory)) {
                return;
            }
            jobCount = static_cast<int>(sqrt(total_memory / 1024) * 4);
        });
        return jobCount;
    }

    /**
     * @brief Handles new standard output data.
     *
     * This function is called when new standard output data is available. It
     * overrides the base class's onNewStdoutBuffer() method to provide custom
     * behavior.
     *
     * @param buffer The buffer containing the new standard output data.
     */
    void onNewStdoutBuffer(ForkAndRun::BufferType& buffer) override {
        std::stringstream buildInfoBuffer;
        const auto now = std::chrono::system_clock::now();
        double memUsage = NAN;
        if (clock + std::chrono::minutes(1) < now) {
            clock = now;
            buildInfoBuffer << "Start time: " << fromTP(startTime) << std::endl;
            buildInfoBuffer << "Time spent: " << to_string(now - startTime)
                            << std::endl;
            buildInfoBuffer << "Last updated on: " << fromTP(now) << std::endl;
            buildInfoBuffer << "Target device: " << data.bConfig.device;
            buildInfoBuffer << "Job count: " << guessJobCount();
            if (_get_used_mem->call(nullptr, &memUsage)) {
                buildInfoBuffer << ", memory usage: " << memUsage << "%";
            }
            switch (data.bConfig.variant) {
                case BuildConfig::Variant::kUser:
                    buildInfoBuffer << ", variant: user";
                    break;
                case BuildConfig::Variant::kUserDebug:
                    buildInfoBuffer << ", variant: userdebug";
                    break;
                case BuildConfig::Variant::kEng:
                    buildInfoBuffer << ", variant: eng";
                    break;
            }
            buildInfoBuffer << std::endl << std::endl;
            bot_editMessage(_bot, message,
                            buildInfoBuffer.str() + buffer.data());
        }
    }

    /**
     * @brief Handles new standard error data.
     *
     * This function is called when new standard error data is available. It
     * overrides the base class's onNewStderrBuffer() method to provide custom
     * behavior.
     *
     * @param buffer The buffer containing the new standard error data.
     */
    void onNewStderrBuffer(ForkAndRun::BufferType& buffer) override {
        std::string lines = buffer.data();
        LOG(ERROR) << "onNewStderr: " << boost::trim_copy(lines);
    }

    /**
     * @brief Handles the process exit event.
     *
     * This function is called when the process exits. It overrides the base
     * class's onExit() method to provide custom behavior.
     *
     * @param exitCode The exit code of the process.
     */
    void onExit(int exitCode) override {
        std::stringstream exitInfoBuffer;
        exitInfoBuffer << "Exit code: " << exitCode;
        bot_editMessage(_bot, message, exitInfoBuffer.str());
        *data.result = exitCode == 0;
        LOG(INFO) << "Process exited with code: " << exitCode;
    }

    /**
     * @brief Handles the process signal event.
     *
     * This function is called when the process receives a signal. It overrides
     * the base class's onSignal() method to provide custom behavior.
     *
     * @param signalCode The signal code that the process received.
     */
    void onSignal(int signalCode) override {
        std::stringstream signalInfoBuffer;
        signalInfoBuffer << "Signal received: " << signalCode;
        bot_editMessage(_bot, message, signalInfoBuffer.str());
        LOG(WARNING) << "Process received signal: " << signalCode;
    }

    /**
     * @brief Constructs a RepoSyncF object with the provided data.
     *
     * This constructor initializes a RepoSyncF object with the given data.
     *
     * @param data The data object containing the necessary configuration and
     * paths.
     */
    explicit ROMBuildTask(const TgBot::Bot& bot, TgBot::Message::Ptr message,
                          PerBuildData data)
        : BotClassBase(bot),
          data(std::move(data)),
          message(std::move(message)) {
        clock = std::chrono::system_clock::now();
        startTime = std::chrono::system_clock::now();

        auto c = PythonClass::get();
        c->addLookupDirectory(data.scriptDirectory);
        auto repomod = c->importModule("system_info");

        if (!repomod) {
            LOG(ERROR) << "Failed to import system_info module";
            return;
        }
        _get_total_mem = repomod->lookupFunction("get_memory_total");
        if (!_get_total_mem) {
            LOG(ERROR) << "Failed to find get_memory_total function";
            return;
        }
        _get_used_mem = repomod->lookupFunction("get_memory_usage");
        if (!_get_used_mem) {
            LOG(ERROR) << "Failed to find get_memory_usage function";
            return;
        }
    }

   private:
    PerBuildData data;
    TgBot::Message::Ptr message;
    std::shared_ptr<PythonClass::FunctionHandle> _get_total_mem;
    std::shared_ptr<PythonClass::FunctionHandle> _get_used_mem;
    decltype(std::chrono::system_clock::now()) clock;
    decltype(std::chrono::system_clock::now()) startTime;
};