#include "ForkAndRun.hpp"

#include <absl/log/log.h>
#include <absl/log/log_sink.h>
#include <absl/log/log_sink_registry.h>
#include <absl/strings/str_split.h>
#include <absl/strings/strip.h>
#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <trivial_helpers/_FileDescriptor_posix.h>
#include <unistd.h>

#include <AbslLogInit.hpp>
#include <LogSinks.hpp>
#include <array>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <libos/libsighandler.hpp>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string_view>
#include <thread>
#include <trivial_helpers/raii.hpp>
#include <utility>
#include <vector>

#include "Shmem.hpp"

struct FDLogSink : public absl::LogSink {
    void Send(const absl::LogEntry& logSink) override {
        if (isWritable) {
            const std::string newLine =
                fmt::format("SubProcess (PID: {}, TID: {}): {}", pid_, gettid(),
                            logSink.text_message_with_prefix_and_newline());
            write(stdout_fd, newLine.c_str(), newLine.size());
        }
    }
    explicit FDLogSink() : stdout_fd(::dup(STDOUT_FILENO)), pid_(getpid()) {
        if (stdout_fd < 0) {
            PLOG(ERROR) << "Failed to duplicate stdout";
            isWritable = false;
        }
    }
    ~FDLogSink() override { ::close(stdout_fd); }

   private:
    int stdout_fd;
    bool isWritable = true;
    pid_t pid_;
};

std::string read_string_from_child(pid_t child_pid, unsigned long addr) {
    std::array<char, PATH_MAX> buf{};
    int i = 0;
    unsigned long word = 0;

    // Read the string word by word
    while (i < buf.size()) {
        errno = 0;
        word = ptrace(PTRACE_PEEKDATA, child_pid, addr + i, NULL);
        if (errno != 0) {
            PLOG(ERROR) << "ptrace(PTRACE_PEEKDATA)";
            break;
        }

        // Copy bytes from the word to the buffer
        memcpy(buf.data() + i, &word, sizeof(word));

        // Check if we've hit the null terminator
        if (memchr(&word, 0, sizeof(word)) != nullptr) {
            break;
        }

        i += sizeof(word);
    }

    // Ensure the buffer is null-terminated
    buf[buf.size() - 1] = '\0';
    return buf.data();
}

DeferredExit ptrace_common_parent(pid_t pid,
                                  const std::filesystem::path& sandboxPath) {
    int status = 0;
    DeferredExit exit;

    LOG(INFO) << "Sandbox dir: " << sandboxPath;

    // Wait for child to stop
    waitpid(pid, &status, 0);

    if (WIFEXITED(status)) {
        LOG(ERROR) << "Child exited too early.";
        exit = DeferredExit(status);
        return std::move(exit);
    }

    // Loop to trace the system calls
    while (true) {
        struct user_regs_struct regs {};
        bool blockReq = false;

        // Step to the next system call entry or exit
        if (ptrace(PTRACE_SYSCALL, pid, NULL, NULL) == -1) {
            PLOG(ERROR) << "ptrace(PTRACE_SYSCALL) failed";
            break;
        }

        // Wait for the child to enter a syscall
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            exit = DeferredExit(status);
            break;
        }

        // syscall has been executed in this stage

        // Step to the syscall exit
        if (ptrace(PTRACE_SYSCALL, pid, NULL, NULL) == -1) {
            PLOG(ERROR) << "ptrace(PTRACE_SYSCALL)";
            break;
        }

        // Wait for the child to exit the syscall
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            exit = DeferredExit(status);
            break;
        }

        // Get the registers again after syscall completes
        if (ptrace(PTRACE_GETREGS, pid, NULL, &regs) == -1) {
            PLOG(ERROR) << "ptrace(PTRACE_GETREGS) failed";
            break;
        }

        // Hooks
        switch (regs.orig_rax) {
            case SYS_newfstatat: {
                std::string buf = read_string_from_child(pid, regs.rsi);
                std::string_view bufView = buf;
                if (absl::ConsumeSuffix(&bufView, ".repo/repo/main.py")) {
                    DLOG(INFO) << "sandboxPath: " << sandboxPath
                              << " accessPath: " << buf;
                    if (!std::filesystem::equivalent(sandboxPath, bufView)) {
                        LOG(WARNING) << "BLOCKING ACCESS TO " << buf;
                        regs.rax = -ENOENT;
                        regs.orig_rax = -1;
                        break;
                    }
                }
                continue;
            }
            default:
                continue;
        }

        if (ptrace(PTRACE_SETREGS, pid, NULL, &regs) == -1) {
            PLOG(ERROR) << "ptrace(PTRACE_SETREGS) failed";
            break;
        }
    }

    // Detach from the child
    ptrace(PTRACE_DETACH, pid, NULL, NULL);
    return std::move(exit);
}

void ptrace_common_child() {
    // Enable tracing
    if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) == -1) {
        PLOG(ERROR) << "ptrace(PTRACE_TRACEME) failed";
        _exit(EXIT_FAILURE);
    }
    // Trigger a stop so the parent can start tracing
    kill(getpid(), SIGSTOP);
}

bool ForkAndRun::execute() {
    Pipe stdout_pipe{};
    Pipe stderr_pipe{};

    if (!stderr_pipe.pipe() || !stdout_pipe.pipe()) {
        stderr_pipe.close();
        stdout_pipe.close();
        PLOG(ERROR) << "Failed to create pipes";
        return false;
    }

    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        {
            DeferredExit exit;
            {
                // Switch to subprocess logging
                TgBot_AbslLogDeInit();
                RAIILogSink<FDLogSink> logSink;

                dup2(stdout_pipe.writeEnd(), STDOUT_FILENO);
                dup2(stderr_pipe.writeEnd(), STDERR_FILENO);
                close(stdout_pipe.readEnd());
                close(stderr_pipe.readEnd());

                // Clear handlers
                SignalHandler::uninstall();

                // Set the process group to the current process ID
                setpgrp();

                // Run the function.
                exit = runFunction();
            }
            // Destructor of exit should exit the process.
        }
        _exit(std::numeric_limits<uint8_t>::max());  // Just in case.
    } else if (pid > 0) {
        static ForkAndRun* instance = nullptr;

        Pipe program_termination_pipe{};
        bool breakIt = false;
        int status = 0;

        instance = this;
        auto sig_fun = [](int signum) {
            LOG(INFO) << "Got signal " << strsignal(signum);
            instance->cancel();
        };
        auto old_sig = signal(SIGINT, sig_fun);
        (void)signal(SIGTERM, sig_fun);

        childProcessId = pid;
        close(stdout_pipe.writeEnd());
        close(stderr_pipe.writeEnd());
        program_termination_pipe.pipe();

        selector.add(
            stdout_pipe.readEnd(),
            [stdout_pipe, this]() {
                BufferType buf{};
                ssize_t bytes_read =
                    read(stdout_pipe.readEnd(), buf.data(), buf.size() - 1);
                if (bytes_read >= 0) {
                    handleStdoutData(buf.data());
                }
            },
            Selector::Mode::READ);
        selector.add(
            stderr_pipe.readEnd(),
            [stderr_pipe, this]() {
                BufferType buf{};
                ssize_t bytes_read =
                    read(stderr_pipe.readEnd(), buf.data(), buf.size() - 1);
                if (bytes_read >= 0) {
                    handleStderrData(buf.data());
                }
            },
            Selector::Mode::READ);
        selector.add(
            program_termination_pipe.readEnd(),
            [program_termination_pipe, &breakIt]() { breakIt = true; },
            Selector::Mode::READ);

        std::thread pollThread([&breakIt, this]() {
            while (!breakIt) {
                switch (selector.poll()) {
                    case Selector::PollResult::OK:
                        break;
                    case Selector::PollResult::FAILED:
                    case Selector::PollResult::TIMEOUT:
                        breakIt = true;
                        break;
                }
            }
        });

        if (waitpid(pid, &status, 0) < 0) {
            PLOG(ERROR) << "Failed to wait for child";
            cancel();
        }

        // Create deferred exit just to dedup code
        auto e = ExitStatusParser::fromStatus(status);
        switch (e.type) {
            case DeferredExit::Type::EXIT:
                onExit(e.code);
                break;
            case DeferredExit::Type::SIGNAL:
                onSignal(e.code);
                break;
            case ExitStatusParser::Type::UNKNOWN:
                LOG(WARNING) << "Unknown exit status";
                break;
        }

        // Notify the polling thread that program has ended.
        write(program_termination_pipe.writeEnd(), &status, sizeof(status));
        pollThread.join();
        childProcessId = -1;

        // Cleanup
        selector.remove(stdout_pipe.readEnd());
        selector.remove(stderr_pipe.readEnd());
        selector.remove(program_termination_pipe.readEnd());
        program_termination_pipe.close();
        stderr_pipe.close();
        stdout_pipe.close();

        // Restore signal handlers
        (void)signal(SIGINT, old_sig);
        (void)signal(SIGTERM, old_sig);
    } else {
        PLOG(ERROR) << "Unable to fork";
    }
    return true;
}

void ForkAndRun::cancel() const {
    if (childProcessId < 0) {
        LOG(WARNING) << "Attempting to cancel non-existing child";
        return;
    }

    killpg(childProcessId, SIGTERM);
}

void ExitStatusParser::update(int status) {
    if (WIFEXITED(status)) {
        type = Type::EXIT;
        code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        type = Type::SIGNAL;
        code = WTERMSIG(status);
    } else {
        type = Type::UNKNOWN;
        code = 0;
    }
}

ExitStatusParser ExitStatusParser::fromStatus(int status) {
    ExitStatusParser exitstatus{};
    exitstatus.update(status);
    return exitstatus;
}

ExitStatusParser ExitStatusParser::fromPid(pid_t pid, bool nohang) {
    int status = 0;
    if (waitpid(pid, &status, nohang ? WNOHANG : 0) < 0) {
        throw syscall_perror("waitpid");
    } else {
        return fromStatus(status);
    }
}

ExitStatusParser::operator bool() const noexcept {
    return type == Type::EXIT && code == 0;
}

DeferredExit::DeferredExit(DeferredExit::fail_t /*unused*/)
    : ExitStatusParser(EXIT_FAILURE, Type::EXIT) {}

DeferredExit::DeferredExit(int status) { update(status); }

DeferredExit::~DeferredExit() {
    if (!destory) {
        return;
    }

    DLOG(INFO) << "At ~DeferredExit";
    if (!*this) {
        DLOG(INFO) << fmt::format("I am a bomb, {}",
                                  *static_cast<ExitStatusParser*>(this));
    }
    switch (type) {
        case Type::EXIT:
            _exit(code);
            break;
        case Type::SIGNAL:
            kill(0, code);
            break;
        case Type::UNKNOWN:
            LOG(WARNING) << "Unknown type: just exiting with status 1";
            _exit(EXIT_FAILURE);
            break;
    }
}

ForkAndRunSimple::ForkAndRunSimple(std::string_view argv)
    : args_(absl::StrSplit(argv, ' ', absl::SkipWhitespace())) {}

DeferredExit ForkAndRunSimple::execute() {
    // Owns the strings
    std::vector<std::string> args;
    args.reserve(args_.size());
    args.insert(args.end(), args_.begin(), args_.end());

    // Convert to raw C-style strings
    std::vector<char*> rawArgs;
    rawArgs.reserve(args.size() + 1);
    for (auto& arg : args) {
        rawArgs.emplace_back(arg.data());
    }
    rawArgs.emplace_back(nullptr);

    // Execute the program
    pid_t pid = fork();
    if (pid == 0) {
        if constexpr (kEnablePtraceHook) {
            ptrace_common_child();
        }

        // Craft envp
        std::vector<char*> envp;
        std::vector<std::string> owners;
        for (int x = 0; environ[x] != nullptr; x++) {
            envp.emplace_back(environ[x]);
        }
        for (const auto& [key, value] : env.map) {
            LOG(INFO) << "Setting env " << key << "=" << value;
            owners.emplace_back(fmt::format("{}={}", key, value));
            envp.emplace_back(owners.back().data());
        }
        envp.emplace_back(nullptr);

        execvpe(args_[0].data(), rawArgs.data(), envp.data());
        _exit(EXIT_FAILURE);
    } else if (pid > 0) {
        if constexpr (kEnablePtraceHook) {
            return ptrace_common_parent(pid, std::filesystem::current_path());
        } else {
            int status = 0;
            if (waitpid(pid, &status, 0) < 0) {
                PLOG(ERROR) << "Failed to wait for shell";
                return DeferredExit::generic_fail;
            }
            return DeferredExit(status);
        }
    } else {
        PLOG(ERROR) << "Unable to fork";
    }
    return DeferredExit::generic_fail;
}

ForkAndRunShell::ForkAndRunShell(std::string shellName)
    : _shellName(std::move(shellName)) {}

bool ForkAndRunShell::open() {
    LOG(INFO) << "Using shell: " << _shellName;

    if (!pipe_.pipe()) {
        PLOG(ERROR) << "Failed to create pipe";
        return false;
    }
    shell_pid_ = fork();
    if (shell_pid_ < 0) {
        PLOG(ERROR) << "Unable to fork shell";
        return false;
    } else if (shell_pid_ == 0) {
        dup2(pipe_.readEnd(), STDIN_FILENO);
        ::close(pipe_.writeEnd());

        if constexpr (kEnablePtraceHook) {
            ptrace_common_child();
        }

        // Craft argv
        auto string =
            RAII<char*>::create<void>(strdup(_shellName.c_str()), free);
        std::array<char*, 2> argv{string.get(), nullptr};

        // Craft envp
        std::vector<char*> envp;
        std::vector<std::string> owners;
        for (int x = 0; environ[x] != nullptr; x++) {
            envp.emplace_back(environ[x]);
        }
        for (const auto& [key, value] : env.map) {
            LOG(INFO) << "Setting env " << key << "=" << value;
            owners.emplace_back(fmt::format("{}={}", key, value));
            envp.emplace_back(owners.back().data());
        }
        envp.emplace_back(nullptr);

        execvpe(string.get(), argv.data(), envp.data());
        _exit(EXIT_FAILURE);
    } else {
        LOG(INFO) << "Shell opened with pid " << shell_pid_;
        ::close(pipe_.readEnd());
        opened = true;
        terminate_watcher_thread = std::thread([this] {
            int status = 0;
            {
                std::shared_lock<std::shared_mutex> pidLk(pid_mutex_);
                if (shell_pid_ < 0) {
                    LOG(INFO) << "shell_pid is negative";
                    return;
                }
                if constexpr (kEnablePtraceHook) {
                    result = ptrace_common_parent(
                        shell_pid_, std::filesystem::current_path());
                } else {
                    if (waitpid(shell_pid_, &status, 0) < 0) {
                        PLOG(ERROR) << "Failed to wait for shell";
                        return;
                    }
                    result = DeferredExit(status);
                }
            }
            // Promote to unique
            {
                std::unique_lock<std::shared_mutex> lock(pid_mutex_);
                shell_pid_ = -1;
            }
        });
    }
    return true;
}

const ForkAndRunShell& ForkAndRunShell::operator<<(
    const endl_t /*unused*/) const {
    writeString("\n");
    return *this;
}

const ForkAndRunShell& ForkAndRunShell::operator<<(
    const suppress_output_t /*unused*/) const {
    // Suppress output
    writeString(" >/dev/null 2>&1");
    return *this;
}

const ForkAndRunShell& ForkAndRunShell::operator<<(
    const and_t /* unused */) const {
    writeString(" &&");
    return *this;
}

const ForkAndRunShell& ForkAndRunShell::operator<<(
    const or_t /* unused */) const {
    writeString(" ||");
    return *this;
}

void ForkAndRunShell::writeString(const std::string_view& str) const {
    const std::shared_lock<std::shared_mutex> _(pid_mutex_);

    if (!opened || shell_pid_ < 0) {
        LOG(ERROR) << "Shell not open. Ignoring " << std::quoted(str);
        return;
    }

    if (::write(pipe_.writeEnd(), str.data(), str.size()) < 0) {
        PLOG(ERROR) << fmt::format("Failed to write '{}' to pipe", str);
    }
}

DeferredExit ForkAndRunShell::close() {
    bool do_wait = false;
    if (!opened) {
        LOG(ERROR) << "Didn't open, so no close.";
        return DeferredExit::generic_fail;
    }
    {
        const std::shared_lock<std::shared_mutex> lock(pid_mutex_);
        if (shell_pid_ >= 0) {
            do_wait = true;
        }
    }
    if (do_wait) {
        *this << "exit" << endl;
        opened = false;  // Prevent future write operations.
    }
    LOG(INFO) << "Exit command written";
    if (do_wait || terminate_watcher_thread.joinable()) {
        terminate_watcher_thread.join();
        ::close(pipe_.writeEnd());
        return std::move(result);
    } else {
        LOG(ERROR) << "Shell not open";
    }
    return DeferredExit::generic_fail;
}