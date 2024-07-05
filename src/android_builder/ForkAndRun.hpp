#pragma once

// Helper class to fork and run a subprocess with stdout/err
#include <unistd.h>

#include <array>
#include <boost/algorithm/string/trim.hpp>
#include <iostream>
#include <string_view>

#include "absl/log/log_entry.h"
#include "absl/log/log_sink.h"
#include "socket/selector/SelectorPosix.hpp"

struct FDLogSink : public absl::LogSink {
    void Send(const absl::LogEntry& logSink) override {
        const auto message = logSink.text_message_with_prefix_and_newline();
        constexpr std::string_view prefix = "SubProcess: ";
        if (isWritable && logSink.log_severity() <= absl::LogSeverity::kWarning) {
            write(stdout_fd, prefix.data(), prefix.size());
            write(stdout_fd, message.data(), message.size());
        }
    }
    explicit FDLogSink() : stdout_fd(::dup(STDOUT_FILENO)) {
        if (stdout_fd < 0) {
            PLOG(ERROR) << "Failed to duplicate stdout";
            isWritable = false;
        }
    }
    ~FDLogSink() override { ::close(stdout_fd); }

   private:
    int stdout_fd;
    bool isWritable = true;
};

class ForkAndRun {
   public:
    virtual ~ForkAndRun() = default;
    /**
     * @brief The size of the buffer used for stdout and stderr.
     */
    constexpr static int kBufferSize = 1024;
    using BufferType = std::array<char, kBufferSize>;

    /**
     * @brief Callback function for handling new stdout data.
     *
     * This method is called whenever new data is available on the stdout of the
     * subprocess.
     * buffer is guaranteed to be a null-terminated string.
     *
     * @param buffer The buffer containing the new stdout data.
     */
    virtual void onNewStdoutBuffer(BufferType& buffer) {}

    /**
     * @brief Callback function for handling new stderr data.
     *
     * This method is called whenever new data is available on the stderr of the
     * subprocess.
     * buffer is guaranteed to be a null-terminated string.
     *
     * @param buffer The buffer containing the new stderr data.
     */
    virtual void onNewStderrBuffer(BufferType& buffer) {
        std::string lines = buffer.data();
        std::cerr << boost::trim_copy(lines) << std::endl;
    }

    /**
     * @brief Callback function for handling the exit of the subprocess.
     *
     * This method is called when the subprocess exits.
     *
     * @param exitCode The exit code of the subprocess.
     */
    virtual void onExit(int exitCode) {}

    /**
     * @brief Callback function for handling the exit of the subprocess.
     *
     * This method is called when the subprocess exits.
     *
     * @param signal The signal that caused the subprocess to exit.
     */
    virtual void onSignal(int signal) {
        onExit(signal);
    }

    /**
     * @brief The function to be run in the subprocess.
     *
     * @return True if the function execution was successful, false otherwise.
     * This method must be overridden in derived classes to specify the function
     * to be run in the subprocess.
     */
    virtual bool runFunction() = 0;

    /**
     * @brief Executes the subprocess with the specified function to be run.
     *
     * @return True if the subprocess execution prepartion was successful, false
     * otherwise. This method forks a new process and runs the function
     * specified in the `runFunction` method of the derived class. It also
     * handles the stdout, stderr, and exit of the subprocess by calling the
     * appropriate callback functions.
     */
    bool execute();

    /**
     * @brief Cancels the execution of the subprocess.
     *
     * This method cancels the execution of the subprocess that was started by
     * the `execute` method. It sets the appropriate flags to indicate that the
     * subprocess should be stopped.
     */
    void cancel();

   private:
    UnixSelector selector;
    pid_t childProcessId = -1;
};