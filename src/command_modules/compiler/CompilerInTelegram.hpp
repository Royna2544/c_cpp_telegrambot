#pragma once

#include <absl/status/status.h>

#include <api/MessageExt.hpp>
#include <filesystem>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include <api/StringResLoader.hpp>
#include "popen_wdt.h"

using TgBot::Message;

struct CompilerInTg {
    /**
     * @brief Abstract interface to handle execution events.
     *
     * This interface defines methods to handle various events
     * that occur during the execution process, such as when
     * an execution starts, finishes, encounters an error,
     * produces results, or times out.
     *
     * ### Typical Call Flow:
     * - **Normal Execution Flow**:
     *   - `onExecutionStarted()` →
     * `onExecutionFinished()` → `onResultReady()`
     * - **Timeout Execution Flow**:
     *   - `onExecutionStarted()` → `onWdtTimeout()` →
     * `onExecutionFinished()` → `onResultReady()`
     * - **Error Execution Flow**:
     *   - `onExecutionStarted()` → `onErrorStatus()`
     */
    struct Interface {
        /**
         * @brief Virtual destructor.
         *
         * Ensures derived classes can be properly destroyed via base pointers.
         */
        virtual ~Interface() = default;

        /**
         * @brief Called when execution of a command starts.
         *
         * @param command A string view of the command that has started
         * execution.
         */
        virtual void onExecutionStarted(const std::string_view& command) = 0;

        /**
         * @brief Called when execution of a command finishes.
         *
         * @param command A string view of the command that has finished
         * execution.
         * @param exit An exit status structure (popen_watchdog_exit_t)
         * indicating the result of the command execution, such as success,
         * failure, or exit code.
         */
        virtual void onExecutionFinished(const std::string_view& command,
                                         const popen_watchdog_exit_t& exit) = 0;

        /**
         * @brief Called when an error status occurs during execution.
         *
         * @param status An absl::Status object representing the error
         * encountered. Provides details of the error, including code and
         * message.
         */
        virtual void onErrorStatus(absl::Status status) = 0;

        /**
         * @brief Called when the result of the execution is ready.
         *
         * @param text A string containing the result text produced by the
         * command.
         */
        virtual void onResultReady(const std::string& text) = 0;

        /**
         * @brief Called when a watchdog timeout occurs.
         *
         * Indicates that the execution process has timed out based on a
         * predefined watchdog timer setting.
         */
        virtual void onWdtTimeout() = 0;
    };

    /**
     * @brief This functions proivides a common interface for the command
     * implementations.
     */
    virtual void run(MessageExt::Ptr message) = 0;

    explicit CompilerInTg(std::unique_ptr<Interface> interface,
                          const StringResLoader::PerLocaleMap* loader);
    virtual ~CompilerInTg() = default;

    /**
     * @brief This function is used to execute a command and return the output.
     *
     * @param cmd The command to be executed.
     * @param res The output of the command.
     * @param use_wdt Whether to use the watchdog to time out the command.
     * If set to false, the function will not time out the command.
     */
    void runCommand(std::string cmd, std::stringstream& res,
                    bool use_wdt = true);

    constexpr static char SPACE = ' ';
    constexpr static std::string_view EMPTY = "(empty)";

   protected:
    std::unique_ptr<Interface> _interface;
    const StringResLoader::PerLocaleMap* _locale;
};

struct CompilerInTgForBash : CompilerInTg {
    CompilerInTgForBash(std::unique_ptr<Interface> interface,
                        const StringResLoader::PerLocaleMap* _loader,
                        bool allowhang)
        : CompilerInTg(std::move(interface), _loader), allowhang(allowhang) {}
    ~CompilerInTgForBash() override = default;
    void run(MessageExt::Ptr message) override;

   private:
    bool allowhang;
};

struct CompilerInTgForGeneric : CompilerInTg {
    struct Params {
        // e.g. /usr/bin/clang (Before the filename)
        std::filesystem::path exe;
        // e.g. /tmp/output.c (Where to write the code to compile/interpret)
        std::filesystem::path outfile;
    };
    explicit CompilerInTgForGeneric(
        std::unique_ptr<Interface> interface,
        const StringResLoader::PerLocaleMap* _locale, Params params)
        : CompilerInTg(std::move(interface), _locale),
          params(std::move(params)) {}

    Params params;
    bool verifyParseWrite(const MessageExt::Ptr& message,
                          std::string& extraargs);
    ~CompilerInTgForGeneric() override = default;
    void run(MessageExt::Ptr message) override;
};

struct CompilerInTgForCCpp : CompilerInTgForGeneric {
    using CompilerInTgForGeneric::CompilerInTgForGeneric;
    using CompilerInTgForGeneric::Params;
    ~CompilerInTgForCCpp() override = default;
    void run(MessageExt::Ptr message) override;
};

// Read buffer size, max allowed buffer size
constexpr const static inline auto BASH_READ_BUF = (1 << 8);
constexpr const static inline auto BASH_MAX_BUF = (1 << 10) * 3;
