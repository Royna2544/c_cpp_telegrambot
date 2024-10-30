#include <absl/log/log.h>
#include <fcntl.h>
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <StructF.hpp>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <filesystem>
#include <limits>
#include <logging/AbslLogInit.hpp>
#include <string_view>
#include <thread>

// Wrapper launcher to create a daemon process from the bot
int main(const int argc, char** argv) {
    const std::filesystem::path kLogFile = fmt::format(
        "log_{:%Y_%m_%d_%H_%M_%S}.txt", std::chrono::system_clock::now());
    TgBot_AbslLogInit();

    if (argc < 2) {
        LOG(INFO) << "A wrapper launcher to create a daemon process.";
        LOG(INFO) << "No bot executable provided.";
        LOG(INFO) << fmt::format("Usage: {} <bot_exe_path> args...", argv[0]);
        return EXIT_FAILURE;
    }

    if (access(argv[1], R_OK | X_OK) != 0) {
        PLOG(ERROR) << "Bot executable not found or not executable.";
        return EXIT_FAILURE;
    }

    LOG(INFO) << fmt::format("Parent pid is {}", getpid());
    static pid_t pid = fork();

    if (pid < 0) {
        PLOG(ERROR) << "Fork failed";
        return EXIT_FAILURE;
    }

    if (pid > 0) {
        exit(EXIT_SUCCESS);  // Parent exits
    }

    // Redirect file descriptors to /dev/null and bot.log
    int devnull_fd = open("/dev/null", O_RDONLY);
    if (devnull_fd < 0) {
        PLOG(ERROR) << "Open /dev/null failed";
        return EXIT_FAILURE;
    }
    int logFile_fd = open(kLogFile.c_str(), O_RDONLY);
    if (logFile_fd > 0) {
        close(logFile_fd);
        // This means the file already exists.
        std::filesystem::remove(kLogFile);
    }
    logFile_fd = open(kLogFile.c_str(), O_CREAT | O_WRONLY, 0644);
    if (logFile_fd < 0) {
        PLOG(ERROR) << "Open bot.log failed";
        return EXIT_FAILURE;
    }

    dup2(devnull_fd, STDIN_FILENO);
    dup2(logFile_fd, STDOUT_FILENO);
    dup2(logFile_fd, STDERR_FILENO);

    // Child process continues
    LOG(INFO) << fmt::format("Daemon process created with PID: {}", getpid());
    LOG(INFO) << fmt::format("Redirecting stdout and stderr to {}",
                             kLogFile.c_str());
    LOG(INFO) << fmt::format("Executable is: {}, argument count: {}", argv[1],
                             argc - 1);

    // Create a new session
    if (setsid() < 0) {
        PLOG(ERROR) << "setsid failed";
        return EXIT_FAILURE;
    }
    // Increment argv to skip the first element
    argv++;
redo_vfork:
    // Second fork to prevent the daemon from acquiring a terminal
    pid = vfork();
    if (pid < 0) {
        PLOG(ERROR) << "Second vfork failed";
        return EXIT_FAILURE;
    }

    if (pid == 0) {
        // Set the process group ID to the same as the process ID
        setpgrp();
        // Execute the bot executable with the provided arguments
        LOG(INFO) << "Executing the child process, " << argv[0] << " with pid "
                  << getpid();
        execvp(argv[0], argv);  // Pass the modified argv to execvp
        _exit(std::numeric_limits<std::uint8_t>::max());
    } else {
        F pidFile;
        constexpr std::string_view kPidFile = "bot.pid";
        if (!pidFile.open(kPidFile, F::Mode::Write)) {
            PLOG(ERROR) << "Failed to open bot.pid";
            return EXIT_FAILURE;
        }
        if (pidFile.puts(fmt::format("{}", getpid())) != F::Result::ok()) {
            LOG(ERROR) << "Failed to write to bot.pid";
            return EXIT_FAILURE;
        }
        pidFile.close();

        // Install signal handlers for termination signals
        const auto handler = [](int signum) {
            LOG(INFO) << "Sending " << strsignal(signum) << " to child process";
            killpg(pid, signum);
            waitpid(pid, nullptr, 0);
            LOG(INFO) << "Exiting the daemon process";
            exit(EXIT_SUCCESS);
        };
        (void)signal(SIGINT, handler);
        (void)signal(SIGTERM, handler);
        (void)signal(SIGHUP, SIG_IGN);  // SIGHUP is ignored in daemon mode

        int status{};
        if (waitpid(pid, &status, 0) < 0) {
            PLOG(ERROR) << "Failed to wait for child";
            return EXIT_FAILURE;
        } else {
            if (WIFEXITED(status)) {
                LOG(INFO) << fmt::format("Child process exited with status: {}",
                                         WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                LOG(INFO) << fmt::format(
                    "Child process terminated by signal: {}", WTERMSIG(status));
            }
        }
        constexpr std::chrono::seconds sleep_secs(10);
        LOG(INFO) << fmt::format("Consumed child death, sleeping for {}", sleep_secs);
        std::this_thread::sleep_for(sleep_secs);
        if (!std::filesystem::exists(kPidFile)) {
            LOG(INFO) << "Pid file gone, exiting";
            return EXIT_SUCCESS;
        }
        std::filesystem::remove(kPidFile);  // Remove the pid file after termination
        goto redo_vfork;  // Restart the daemon process loop
    }
}
