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
#include <functional>
#include <iostream>
#include <shared_mutex>
#include <socket/selector/SelectorPosix.hpp>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>

class ExitStatusParser {
   protected:
    void update(int status);
    ExitStatusParser() = default;

   public:
    enum class Type { UNKNOWN, EXIT, SIGNAL };

    // Returns true if the process has exited with code 0
    explicit operator bool() const noexcept;

    // Default ctor
    ExitStatusParser(int code, Type type) : code(code), type(type) {}

    // Calls a waitpid on the pid, and returns ExitStatusParser
    static ExitStatusParser fromPid(pid_t pid, bool nohang = false);

    // Create a ExitStatusParser with the given status.
    static ExitStatusParser fromStatus(int status);

    Type type{};
    int code{};
};

// Specialize fmt::formatter for EngineResult
template <>
struct fmt::formatter<ExitStatusParser> {
    static constexpr auto parse(fmt::format_parse_context& ctx) {
        return ctx.begin();
    }

    static auto format(const ExitStatusParser& status,
                       fmt::format_context& ctx) {
        std::string_view typeStr;
        switch (status.type) {
            case ExitStatusParser::Type::EXIT:
                typeStr = "EXIT";
                break;
            case ExitStatusParser::Type::SIGNAL:
                typeStr = "SIGNAL";
                break;
            case ExitStatusParser::Type::UNKNOWN:
                typeStr = "UNKNOWN";
                break;
        };
        return fmt::format_to(ctx.out(), "ExitStatus [code {}, type {}]",
                              status.code, typeStr);
    }
};

struct DeferredExit : ExitStatusParser {
    struct fail_t {};
    constexpr static inline fail_t generic_fail{};

    // Create a deferred exit with the given status.
    explicit DeferredExit(int status);
    // Create a default-failure
    DeferredExit(fail_t);
    // Re-do the deferred exit.
    ~DeferredExit();
    // Default ctor
    DeferredExit() = default;

    NO_COPY_CTOR(DeferredExit);

    // Move operator
    DeferredExit(DeferredExit&& other) noexcept : ExitStatusParser(other) {
        other.destroy = false;
    }
    DeferredExit& operator=(DeferredExit&& other) noexcept {
        if (this != &other) {
            ExitStatusParser::operator=(other);
            other.destroy = false;
        }
        return *this;
    }

    void defuse() { destroy = false; }
    bool destroy = true;
};

namespace details {

template <typename... Bases>
struct overload : Bases... {
    using is_transparent = void;
    using Bases::operator()...;
};

struct char_pointer_hash {
    auto operator()(const char* ptr) const noexcept {
        return std::hash<std::string_view>{}(ptr);
    }
};

using transparent_string_hash =
    overload<std::hash<std::string>, std::hash<std::string_view>,
             char_pointer_hash>;
}  // namespace details

class ForkAndRun {
   public:
    virtual ~ForkAndRun() = default;

    static bool can_execve(const std::string_view name);

    /**
     * @brief The size of the buffer used for stdout and stderr.
     */
    constexpr static int kBufferSize = 1024;
    using BufferViewType = std::string_view;
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
    virtual void handleStdoutData(BufferViewType buffer) {
        std::cout << buffer;
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
    virtual void handleStderrData(BufferViewType buffer) {
        std::cerr << buffer;
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

    class Env {
        std::unordered_map<std::string, std::string,
                           details::transparent_string_hash, std::equal_to<>>
            map;

       public:
        Env() = default;
        friend class ForkAndRunShell;
        friend class ForkAndRunSimple;

        struct ValueEntry {
            decltype(map)::pointer _it;
            Env* _env;

            ValueEntry(Env* env, decltype(map)::pointer it)
                : _it(it), _env(env) {}

            void operator=(const std::string_view other) const&& {
                _it->second.assign(other);
            }
            void operator+=(const std::string_view other) const&& {
                _it->second.append(other);
            }

            void clear() const&& { _env->erase(_it->first); }

            // Do not allow copying of this element outside the function use.
            NO_COPY_CTOR(ValueEntry);
            NO_MOVE_CTOR(ValueEntry);
            ValueEntry() = delete;
            ~ValueEntry() = default;
        };

        ValueEntry operator[](const std::string_view key);

        void erase(const std::string_view key);

        std::pair<std::vector<char*>, std::vector<std::string>> craft() const;
    };

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
    explicit ForkAndRunSimple(std::string_view argv);
    static constexpr bool kEnablePtraceHook = true;

    ForkAndRun::Env env;

    // Runs the argv, and returns the result.
    [[nodiscard]] DeferredExit execute();
};

template <typename T>
concept WriteableToStdOstream = requires(T t) { std::cout << t; };

class ForkAndRunShell {
    std::string _shellName;
    Pipe pipe_{};

    // Protect shell_pid_, and if process terminated, we wont want write() to
    // block.
    mutable std::shared_mutex pid_mutex_;
    pid_t shell_pid_ = -1;

    // terminate watcher
    std::thread terminate_watcher_thread;
    DeferredExit result;

    bool opened = false;

    void writeString(const std::string_view& args) const;

   public:
    constexpr static std::string_view DefaultShell = "bash";
    explicit ForkAndRunShell(std::string shellName = DefaultShell.data());
    static constexpr bool kEnablePtraceHook = false;

    // Tag objects
    struct endl_t {};
    static constexpr endl_t endl{};
    struct suppress_output_t {};
    static constexpr suppress_output_t suppress_output{};
    struct and_t {};
    static constexpr and_t andl{};
    struct or_t {};
    static constexpr or_t orl{};

    ForkAndRun::Env env;

    bool open();

    // Write arguments to the shell
    template <WriteableToStdOstream T>
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

    // Support ForkAndRunShell::suppress_output
    const ForkAndRunShell& operator<<(const suppress_output_t) const;

    // Support ForkAndRunShell::and
    const ForkAndRunShell& operator<<(const and_t) const;

    // Support ForkAndRunShell::or
    const ForkAndRunShell& operator<<(const or_t) const;

    // waits for anything and either exits or kills itself, no return.
    [[nodiscard]] DeferredExit close();
};
