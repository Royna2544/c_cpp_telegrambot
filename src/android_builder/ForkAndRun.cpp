#include "ForkAndRun.hpp"

#include <absl/log/log.h>
#include <internal/_FileDescriptor_posix.h>
#include <sys/wait.h>

#include <cstdlib>
#include <cstring>
#include <thread>
#include <dlfcn.h>

bool ForkAndRun::execute() {
    Pipe stdout_pipe{};
    Pipe stderr_pipe{};

    if (!stderr_pipe.pipe() || !stdout_pipe.pipe()) {
        PLOG(ERROR) << "Failed to create pipes";
        return false;
    }
    pid_t pid = fork();
    if (pid == 0) {
        dup2(stdout_pipe.writeEnd(), STDOUT_FILENO);
        dup2(stderr_pipe.writeEnd(), STDERR_FILENO);
        close(stdout_pipe.readEnd());
        close(stderr_pipe.readEnd());
        if (runFunction()) {
            _exit(EXIT_SUCCESS);
        } else {
            _exit(EXIT_FAILURE);
        }
    } else if (pid > 0) {
        Pipe program_termination_pipe{};
        bool breakIt = false;
        int status = 0;
        
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

        // Cleanup
        program_termination_pipe.close();
        stderr_pipe.close();
        stdout_pipe.close();
    } else {
        PLOG(ERROR) << "Unable to fork";
    }
    return true;
}

void ForkAndRun::cancel() {
    if (childProcessId > 0) {
        kill(childProcessId, SIGINT);
    }
    // Wait for the child process to terminate.
    int status = 0;
    waitpid(childProcessId, &status, 0);
    if (WIFSIGNALED(status)) {
        LOG(INFO) << "Subprocess terminated by signal: " << strsignal(WTERMSIG(status));
    } else {
        LOG(WARNING) << "Unexpected status: " << status;
    }
    childProcessId = -1;
}