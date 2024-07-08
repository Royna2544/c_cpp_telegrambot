#pragma once

// Helper class to fork and run a subprocess with stdout/err
#include <sys/types.h>
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
        if (isWritable) {
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
    virtual void onSignal(int signal) { onExit(signal); }

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

    struct Shmem {
        std::string path;
        off_t size;
        void* memory;
        bool isAllocated;
    };

    /**
     * @brief Allocates shared memory for the subprocess.
     *
     * This method allocates shared memory for the subprocess using the
     * specified path and size. It returns a pointer to the allocated memory.
     *
     * @param path The path to the shared memory segment.
     * @param size The size of the shared memory segment in bytes.
     *
     * @return A Shmem object containing the allocated shared memory segment,
     * or std::nullopt if the allocation failed.
     */
    static std::optional<Shmem> allocShmem(const std::string_view& path,
                                           off_t size);

    /**
     * @brief Frees the shared memory allocated for the subprocess.
     *
     * This method frees the shared memory that was allocated for the subprocess
     * using the `allocShmem` method. It takes a pointer to the allocated memory
     * as an argument and frees the memory associated with it.
     *
     * @param shmem The Shmem object containing the allocated shared memory
     * segment.
     */
    static void freeShmem(Shmem& shmem);

    /**
     * @brief Connects to a shared memory segment.
     *
     * This method connects to a shared memory segment specified by the given
     * path. It returns a pointer to the allocated shared memory segment.
     *
     * @param path The path to the shared memory segment.
     * @param size The size of the shared memory segment.
     *
     * @return A Shmem object containing the connected shared memory segment,
     * or std::nullopt if the connection failed.
     */
    static std::optional<Shmem> connectShmem(const std::string_view& path,
                                             const off_t size);

    static void disconnectShmem(Shmem& shmem);

   private:
    UnixSelector selector;
    pid_t childProcessId = -1;
};