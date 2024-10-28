#include "ForkAndRun.hpp"

#include <absl/log/log.h>
#include <absl/log/log_sink.h>
#include <absl/log/log_sink_registry.h>
#include <fmt/format.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <trivial_helpers/_FileDescriptor_posix.h>
#include <trivial_helpers/_class_helper_macros.h>
#include <unistd.h>

#include <AbslLogInit.hpp>
#include <LogSinks.hpp>
#include <Random.hpp>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <libos/libsighandler.hpp>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string_view>
#include <thread>
#include <trivial_helpers/raii.hpp>
#include <type_traits>
#include <utility>
#include <vector>

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
        TgBot_AbslLogDeInit();
        {
            DeferredExit exit;
            {
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
        static void (*old_sighandler)(int) = nullptr;

        Pipe program_termination_pipe{};
        bool breakIt = false;
        int status = 0;

        instance = this;
        auto sig_fun = [](int signum) {
            LOG(INFO) << "Got signal " << strsignal(signum);
            instance->cancel();
            if (old_sighandler != SIG_DFL && old_sighandler != SIG_IGN) {
                old_sighandler(signum);
            }
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
                    onNewStdoutBuffer(buf);
                    buf.fill(0);
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
                    onNewStderrBuffer(buf);
                    buf.fill(0);
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
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            onExit(WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            onSignal(WTERMSIG(status));
        } else {
            LOG(WARNING) << "Unknown program termination: " << status;
            onExit(0);
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

DeferredExit::DeferredExit(DeferredExit::fail_t /*unused*/)
    : type(Type::EXIT), code(EXIT_FAILURE) {}

DeferredExit::DeferredExit(int status) {
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

DeferredExit::~DeferredExit() {
    if (!destory) {
        return;
    }

    DLOG(INFO) << "At ~DeferredExit";
    if (!*this) {
        std::string_view typeStr;
        switch (type) {
            case Type::EXIT:
                typeStr = "EXIT";
                break;
            case Type::SIGNAL:
                typeStr = "SIGNAL";
                break;
            case Type::UNKNOWN:
                typeStr = "UNKNOWN";
                break;
        };
        DLOG(INFO) << fmt::format("I am a bomb, I contain code {} and type {}",
                                  code, typeStr);
    }
    switch (type) {
        case Type::EXIT:
            _exit(code);
            break;
        case Type::SIGNAL:
            kill(0, code);
            break;
        case Type::UNKNOWN:
            LOG(WARNING)
                << "Deferred exit: unknown type. Take default action: exit 1";
            _exit(EXIT_FAILURE);
            break;
    }
}

DeferredExit::operator bool() const noexcept {
    return type == Type::EXIT && code == 0;
}

ForkAndRunSimple::ForkAndRunSimple(std::vector<std::string> argv)
    : args_(std::move(argv)) {}

DeferredExit ForkAndRunSimple::operator()() {
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
    pid_t pid = vfork();
    if (pid == 0) {
        execvp(args_[0].data(), rawArgs.data());
        _exit(EXIT_FAILURE);
    } else if (pid > 0) {
        int status = 0;
        if (waitpid(pid, &status, 0) < 0) {
            PLOG(ERROR) << "Failed to wait for child";
            return DeferredExit::generic_fail;
        }
        return DeferredExit{status};
    } else {
        PLOG(ERROR) << "Unable to fork";
    }
    return DeferredExit::generic_fail;
}

ForkAndRunShell::ForkAndRunShell(std::filesystem::path shell_path)
    : shell_path_(std::move(shell_path)) {}

void ForkAndRunShell::addEnv(
    const std::initializer_list<std::pair<std::string_view, std::string_view>>&
        list) {
    if (list.size() == 0) {
        return;
    }
    envMap.insert(list.begin(), list.end());
}

bool ForkAndRunShell::open() {
    LOG(INFO) << "Using shell: " << shell_path_;

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

        // Craft argv
        auto string = RAII<char*>::create<void>(
            strdup(shell_path_.string().c_str()), free);
        std::array<char*, 2> argv{string.get(), nullptr};

        // Craft envp (string_view as we don't have to copy the environment
        // strings)
        std::vector<char*> envp;
        std::vector<std::string> owners;
        for (int x = 0; environ[x] != nullptr; x++) {
            envp.emplace_back(environ[x]);
        }
        for (const auto& [key, value] : envMap) {
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
                if (waitpid(shell_pid_, &status, 0) < 0) {
                    PLOG(ERROR) << "Failed to wait for shell";
                    return;
                }
            }
            // Promote to unique
            {
                std::unique_lock<std::shared_mutex> lock(pid_mutex_);
                shell_pid_ = -1;
            }
            result = DeferredExit{status};
        });
    }
    return true;
}

const ForkAndRunShell& ForkAndRunShell::operator<<(
    const endl_t /*unused*/) const {
    writeString("\n");
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
    *this << "exit" << endl;
    opened = false;  // Prevent future write operations.
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