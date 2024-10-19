#pragma once

// Helper class to fork and run a subprocess with stdout/err
#include <absl/log/log_entry.h>
#include <absl/log/log_sink.h>
#include <absl/strings/ascii.h>
#include <fmt/format.h>
#include <sys/types.h>
#include <trivial_helpers/_FileDescriptor_posix.h>
#include <trivial_helpers/_class_helper_macros.h>
#include <unistd.h>

#include <array>
#include <filesystem>
#include <initializer_list>
#include <iostream>
#include <map>
#include <shared_mutex>
#include <socket/selector/SelectorPosix.hpp>
#include <string_view>
#include <thread>

struct DeferredExit {
    struct fail_t {};
    constexpr static inline fail_t generic_fail{};

    // Create a deferred exit with the given status.
    explicit DeferredExit(int status);
    // Create a default-failure
    DeferredExit(fail_t);
    // Re-do the deferred exit.
    ~DeferredExit();
    // Default ctor
    DeferredExit() : code(0), type(Type::EXIT) {}

    NO_COPY_CTOR(DeferredExit);

    // Move operator
    DeferredExit(DeferredExit&& other) noexcept
        : type(other.type), code(other.code) {
        other.destory = false;
    }
    DeferredExit& operator=(DeferredExit&& other) noexcept {
        if (this != &other) {
            type = other.type;
            code = other.code;
            other.destory = false;
        }
        return *this;
    }

    void defuse() { destory = false; }
    operator bool() const noexcept;

    enum class Type { UNKNOWN, EXIT, SIGNAL } type;
    int code;
    bool destory = true;
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
    virtual void onNewStdoutBuffer(BufferType& buffer) {
        std::string lines = buffer.data();
        std::cout << absl::StripAsciiWhitespace(lines) << std::endl;
    }

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
        std::cerr << absl::StripAsciiWhitespace(lines) << std::endl;
    }

    /**
     * @brief Callback function for handling the exit of the subprocess.
     *
     * This method is called when the subprocess exits.
     *
     * @param exitCode The exit code of the subprocess.
     */
    virtual void onExit(int exitCode) {
        LOG(INFO) << "Process exits with exit code " << exitCode;
    }

    /**
     * @brief Callback function for handling the exit of the subprocess.
     *
     * This method is called when the subprocess exits.
     *
     * @param signal The signal that caused the subprocess to exit.
     */
    virtual void onSignal(int signal) {
        LOG(INFO) << "Process exits due to signal " << signal;
    }

    /**
     * @brief Pure virtual function to be implemented by derived classes.
     * This function is responsible for the main logic of the subprocess to be
     * run.
     *
     * DeferredExit is used to delay the process exit.
     *
     * @return A DeferredExit object indicating the exit status of the
     * subprocess.
     *         - DeferredExit::success: Indicates successful exit of the
     * subprocess.
     *         - DeferredExit::generic_fail: Indicates a generic failure in the
     * subprocess.
     *         - DeferredExit(int status): Indicates an exit with a specific
     * exit code.
     */
    virtual DeferredExit runFunction() = 0;

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
    void cancel() const;

   private:
    UnixSelector selector;
    pid_t childProcessId = -1;
};

inline std::ostream& operator<<(std::ostream& os, DeferredExit const& df) {
    if (df.type == DeferredExit::Type::EXIT) {
        os << "Deferred exit: " << df.code;
    } else if (df.type == DeferredExit::Type::SIGNAL) {
        os << "Deferred signal: " << df.code;
    } else {
        os << "Deferred exit: (unknown type)";
    }
    return os;
}

class ForkAndRunSimple {
    std::vector<std::string> args_;

   public:
    explicit ForkAndRunSimple(std::vector<std::string> argv);

    // Runs the argv, and either exits or kills itself, no return.
    [[nodiscard]] DeferredExit operator()();
};

template <typename T>
concept WriteableToStdOstream = requires(T t) { std::cout << t; };

class ForkAndRunShell {
    std::filesystem::path shell_path_;
    Pipe pipe_{};

    // Protect shell_pid_, and if process terminated, we wont want write() to
    // block.
    mutable std::shared_mutex pid_mutex_;
    pid_t shell_pid_ = -1;

    // terminate watcher
    std::thread terminate_watcher_thread;
    DeferredExit result;

    bool opened = false;
    std::map<std::string_view, std::string_view> envMap;

    void writeString(const std::string_view& args) const;

   public:
    explicit ForkAndRunShell(std::filesystem::path shell_path);

    // Tag object
    struct endl_t {};
    static constexpr endl_t endl{};

    bool open();
    void addEnv(const std::initializer_list<
                std::pair<std::string_view, std::string_view>>& list);

    // Write arguments to the shell
    template <typename T>
        requires WriteableToStdOstream<T>
    const ForkAndRunShell& operator<<(const T& args) const {
        if constexpr (std::is_constructible_v<std::string_view, T>) {
            writeString(args);
        } else if constexpr (std::is_same_v<T, std::filesystem::path>) {
            writeString(args.string());
        } else {
            writeString(fmt::to_string(args));
        }
        return *this;
    }

    // Support ForkAndRunShell::endl
    const ForkAndRunShell& operator<<(const endl_t) const;

    // waits for anything and either exits or kills itself, no return.
    [[nodiscard]] DeferredExit close();
};