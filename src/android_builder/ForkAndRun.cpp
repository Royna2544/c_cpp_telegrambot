#include "ForkAndRun.hpp"

#include <Python.h>
#include <absl/log/log.h>
#include <absl/log/log_sink_registry.h>
#include <fcntl.h>
#include <internal/_FileDescriptor_posix.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <boost/algorithm/string/trim.hpp>
#include <csignal>
#include <cstdlib>
#include <libos/OnTerminateRegistrar.hpp>
#include <optional>
#include <thread>

#include "PythonClass.hpp"
#include "RandomNumberGenerator.hpp"
#include "libos/libsighandler.hpp"

void sigchld_handler(int) {
    while (waitpid(-1, nullptr, WNOHANG) > 0) {
    }
}

bool ForkAndRun::execute() {
    Pipe stdout_pipe{};
    Pipe stderr_pipe{};
    Pipe python_pipe{};

    if (!stderr_pipe.pipe() || !stdout_pipe.pipe() || !python_pipe.pipe()) {
        stderr_pipe.close();
        stdout_pipe.close();
        python_pipe.close();
        PLOG(ERROR) << "Failed to create pipes";
        return false;
    }

    auto* mainTS = PyEval_SaveThread();
    pid_t pid = fork();
    if (pid == 0) {
        FDLogSink sink;
        absl::AddLogSink(&sink);
        dup2(stdout_pipe.writeEnd(), STDOUT_FILENO);
        dup2(stderr_pipe.writeEnd(), STDERR_FILENO);
        close(stdout_pipe.readEnd());
        close(stderr_pipe.readEnd());
        close(python_pipe.readEnd());

        // Clear handlers
        SignalHandler::uninstall();

        // Handle sigchld
        struct sigaction sa {};
        sa.sa_handler = sigchld_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART;
        sigaction(SIGCHLD, &sa, nullptr);

        // Set the process group to the current process ID
        setpgrp();
    
        // Append PYTHON LOG FD
        PyEval_RestoreThread(mainTS);
        {
            GILStateManagement _gil;
            PyObject* os = PyImport_ImportModule("os");
            PyObject* os_environ = PyObject_GetAttrString(os, "environ");
            PyObject* value = PyUnicode_FromString(
                std::to_string(python_pipe.writeEnd()).c_str());
            PyMapping_SetItemString(os_environ, "PYTHON_LOG_FD", value);

            // Clean up
            Py_DECREF(value);
            Py_DECREF(os_environ);
            Py_DECREF(os);
        }

        int ret = 0;
        ret = runFunction() ? EXIT_SUCCESS : EXIT_FAILURE;
        absl::RemoveLogSink(&sink);
        _exit(ret);
    } else if (pid > 0) {
        Pipe program_termination_pipe{};
        bool breakIt = false;
        int status = 0;
        random_return_type token = RandomNumberGenerator::generate(100);
        auto tregi = OnTerminateRegistrar::getInstance();

        tregi->registerCallback([this]() { cancel(); }, token);

        childProcessId = pid;
        close(stdout_pipe.writeEnd());
        close(stderr_pipe.writeEnd());
        close(python_pipe.writeEnd());
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
            python_pipe.readEnd(),
            [python_pipe, this] {
                BufferType buf{};
                ssize_t bytes_read =
                    read(python_pipe.readEnd(), buf.data(), buf.size() - 1);
                if (bytes_read >= 0) {
                    std::cout << "Python output: "
                              << boost::trim_copy(std::string(buf.data()))
                              << std::endl;
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

        PyEval_RestoreThread(mainTS);
        // Notify the polling thread that program has ended.
        write(program_termination_pipe.writeEnd(), &status, sizeof(status));
        pollThread.join();
        childProcessId = -1;

        // Cleanup
        selector.remove(stdout_pipe.readEnd());
        selector.remove(stderr_pipe.readEnd());
        selector.remove(program_termination_pipe.readEnd());
        selector.remove(python_pipe.readEnd());
        program_termination_pipe.close();
        stderr_pipe.close();
        stdout_pipe.close();
        python_pipe.close();
        tregi->unregisterCallback(token);
    } else {
        PLOG(ERROR) << "Unable to fork";
    }
    return true;
}

void ForkAndRun::cancel() {
    if (childProcessId < 0) {
        PLOG(WARNING) << "Attempting to cancel non-existing child";
        return;
    }

    killpg(childProcessId, SIGTERM);

    // Wait for the child process to terminate.
    int status = 0;
    // The canceler should not be blocked by the signal handler.
    pid_t pid = waitpid(childProcessId, &status, WNOHANG);
    if (pid == 0) {
        PLOG(WARNING) << "Child still running after timeout";
        return;
    }
    if (WIFSIGNALED(status)) {
        LOG(INFO) << "Subprocess terminated by signal: "
                  << strsignal(WTERMSIG(status));
    } else {
        LOG(WARNING) << "Unexpected status: " << status;
    }
    childProcessId = -1;
}

std::optional<ForkAndRun::Shmem> ForkAndRun::allocShmem(
    const std::string_view& path, off_t size) {
    void* ptr = nullptr;
    int fd = shm_open(path.data(), O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        PLOG(ERROR) << "shm_open failed";
        return std::nullopt;
    }
    const auto fdCloser = createFdAutoCloser(&fd);
    if (ftruncate(fd, size) == -1) {
        PLOG(ERROR) << "ftruncate failed";
        return std::nullopt;
    }
    ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        PLOG(ERROR) << "mmap failed";
        return std::nullopt;
    }
    DLOG(INFO) << "Shmem created with path: " << path.data()
               << " size: " << size << " bytes";
    return Shmem{path.data(), size, ptr, true};
}

void ForkAndRun::freeShmem(Shmem& shmem) {
    if (munmap(shmem.memory, shmem.size) == -1) {
        PLOG(ERROR) << "munmap failed";
    }
    if (shm_unlink(shmem.path.c_str()) == -1) {
        PLOG(ERROR) << "shm_unlink failed";
    }
    DLOG(INFO) << "Shmem freed";
    shmem.isAllocated = false;
}

std::optional<ForkAndRun::Shmem> ForkAndRun::connectShmem(
    const std::string_view& path, const off_t size) {
    int fd = shm_open(path.data(), O_RDWR, 0);
    if (fd == -1) {
        PLOG(ERROR) << "shm_open failed";
        return std::nullopt;
    }
    const auto fdCloser = createFdAutoCloser(&fd);
    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        PLOG(ERROR) << "mmap failed";
        return std::nullopt;
    }
    DLOG(INFO) << "Shmem connected with path: " << path.data();
    return Shmem{path.data(), size, ptr, true};
}

void ForkAndRun::disconnectShmem(Shmem& shmem) {
    if (munmap(shmem.memory, shmem.size) == -1) {
        PLOG(ERROR) << "munmap failed";
    }
    DLOG(INFO) << "Shmem disconnected";
    shmem.isAllocated = false;
}