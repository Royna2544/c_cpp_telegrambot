#include "ForkAndRun.hpp"

#include <absl/log/log.h>
#include <absl/log/log_sink_registry.h>
#include <absl/strings/ascii.h>
#include <fcntl.h>
#include <internal/_FileDescriptor_posix.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <Random.hpp>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <internal/raii.hpp>
#include <libos/OnTerminateRegistrar.hpp>
#include <libos/libsighandler.hpp>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include "AbslLogInit.hpp"

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
        FDLogSink sink;
        TgBot_AbslLogDeInit();
        absl::AddLogSink(&sink);
    
        dup2(stdout_pipe.writeEnd(), STDOUT_FILENO);
        dup2(stderr_pipe.writeEnd(), STDERR_FILENO);
        close(stdout_pipe.readEnd());
        close(stderr_pipe.readEnd());

        // Clear handlers
        SignalHandler::uninstall();

        // Set the process group to the current process ID
        setpgrp();

        int ret = 0;
        ret = runFunction() ? EXIT_SUCCESS : EXIT_FAILURE;
        absl::RemoveLogSink(&sink);
        _exit(ret);
    } else if (pid > 0) {
        Pipe program_termination_pipe{};
        bool breakIt = false;
        int status = 0;
        auto token = Random::getInstance()->generate(100);
        auto tregi = OnTerminateRegistrar::getInstance();

        tregi->registerCallback([this]() { cancel(); }, token);

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
        tregi->unregisterCallback(token);
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
    : type(Type::EXIT), code(1) {}

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
    if (*this) {
        DLOG(INFO) << "Skip the deferred exit";
        return;
    }
    switch (type) {
        case Type::EXIT:
            _exit(code);
            break;
        case Type::SIGNAL:
            kill(0, code);
            break;
        case Type::UNKNOWN:
            LOG(WARNING) << "Deferred exit: unknown type. BYdef-exit1";
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
        auto stringLen = shell_path_.string().size();
        auto ptr = std::make_unique_for_overwrite<char[]>(stringLen + 1);
        std::strcpy(ptr.get(), shell_path_.c_str());
        ptr.get()[stringLen] = 0;
        char* argv[] = {ptr.get(), nullptr};
        execvp(ptr.get(), argv);
        _exit(EXIT_FAILURE);
    } else {
        LOG(INFO) << "Shell opened with pid " << shell_pid_;
        ::close(pipe_.readEnd());
        opened = true;
        terminate_watcher_thread = std::thread([this] {
            int status = 0;
            while (true) {
                {
                    std::shared_lock<std::shared_mutex> pidLk(pid_mutex_);
                    if (waitpid(shell_pid_, &status, 0) < 0) {
                        PLOG(ERROR) << "Failed to wait for shell";
                        break;
                    }
                }
                // Promote to unique
                {
                    std::unique_lock<std::shared_mutex> lock(pid_mutex_);
                    shell_pid_ = -1;
                }
                result = DeferredExit{status};
            }
        });
    }
    return true;
}

const ForkAndRunShell& ForkAndRunShell::operator<<(
    const endl_t /*unused*/) const {
    const std::shared_lock<std::shared_mutex> _(pid_mutex_);

    if (!opened || shell_pid_ < 0) {
        LOG(ERROR) << "Shell not open. Ignoring endl";
        return *this;
    }
    write(pipe_.writeEnd(), "\n", 1);
    return *this;
}

void ForkAndRunShell::writeString(const std::string& args) const {
    const std::shared_lock<std::shared_mutex> _(pid_mutex_);

    if (!opened || shell_pid_ < 0) {
        LOG(ERROR) << "Shell not open. Ignoring " << std::quoted(args);
        return;
    }
    write(pipe_.writeEnd(), args.data(), args.size());
}

DeferredExit ForkAndRunShell::close() {
    bool do_wait = false;
    if (!opened) {
        LOG(ERROR) << "Didn't open, so no close.";
        return DeferredExit::generic_fail;
    }
    {
        const std::unique_lock<std::shared_mutex> lock(pid_mutex_);
        if (shell_pid_ >= 0) {
            do_wait = true;
        }
    }
    opened = false;  // Prevent future write operations.
    LOG(INFO) << "Writing exit command";
    const std::string_view eof = "exit\n";
    const auto rc =
        write(pipe_.writeEnd(), eof.data(), eof.size());  // EOF to shell
    if (rc < 0) {
        PLOG(ERROR) << "Failed to write exit command";
        return DeferredExit::generic_fail;
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