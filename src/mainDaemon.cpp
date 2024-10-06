#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>

// Wrapper launcher to create a daemon process from the bot
int main(const int argc, char** argv) {
    std::array<char, std::numeric_limits<pid_t>::digits10 + 1> kLogFile{};
    snprintf(kLogFile.data(), sizeof(kLogFile) - 1, "log_%d.txt", getpid());

    if (argc < 2) {
        puts("A wrapper launcher to create a daemon process.");
        puts("No bot executable provided.");
        printf("Usage: %s <bot_exe_path> args...\n", argv[0]);
        return EXIT_FAILURE;
    }
    printf("My pid is %d\n", getpid());

    pid_t pid = fork();

    if (pid < 0) {
        perror("Fork failed");
        return EXIT_FAILURE;
    }

    if (pid > 0) {
        exit(EXIT_SUCCESS);  // Parent exits
    }

    // Child process continues
    printf("Daemon process created with PID: %d\n", getpid());
    printf("Redirecting stdout and stderr to %s\n", kLogFile.data());
    printf("Executable is: %s, argument count: %d\n", argv[1], argc - 1);

    if (access(argv[1], R_OK | X_OK) != 0) {
        fprintf(stderr, "Error: Bot executable not found or not executable.\n");
        return EXIT_FAILURE;
    }

    // Create a new session
    if (setsid() < 0) {
        perror("setsid failed");
        return EXIT_FAILURE;
    }

    // Optional: Second fork to prevent the daemon from acquiring a terminal
    pid = fork();
    if (pid < 0) {
        perror("Second fork failed");
        return EXIT_FAILURE;
    }

    if (pid > 0) {
        exit(EXIT_SUCCESS);  // Parent exits
    }

    // Skip changing working directory to root directory

    // Close all open file descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    if (access(kLogFile.data(), F_OK) == 0) {
        puts("Removing old log file");
        unlink(kLogFile.data());  // Remove old log file
    }

    // Redirect file descriptors to /dev/null and bot.log
    if (!freopen("/dev/null", "r", stdin)) {
        perror("freopen stdin failed");
        return EXIT_FAILURE;
    }
    if (!freopen(kLogFile.data(), "a+", stdout)) {
        perror("freopen stdout failed");
        return EXIT_FAILURE;
    }
    if (!freopen(kLogFile.data(), "a+", stderr)) {
        perror("freopen stderr failed");
        return EXIT_FAILURE;
    }

    // Set the process group ID to the same as the process ID
    setpgid(0, 0);

    // Run the bot in the background
    // Increment argv to skip the first element
    argv++;
    return execvp(argv[0], argv);  // Pass the modified argv to execvp
}
