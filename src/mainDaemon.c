#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// Wrapper launcher to create a daemon process from the bot
int main(const int argc, char** argv) {
    if (argc < 2) {
        puts("A wrapper launcher to create a daemon process.");
        puts("No bot executable provided.");
        printf("Usage: %s <bot_exe_path> args...\n", argv[0]);
        return EXIT_FAILURE;
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("Fork failed");
        return EXIT_FAILURE;
    }

    if (pid > 0) {
        exit(EXIT_SUCCESS);  // Parent exits
    }

    // Child process continues

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

    // Set file creation mask to 0
    umask(0);

    // Skip changing working directory to root directory

    // Close all open file descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    // Redirect file descriptors to /dev/null and bot.log
    if (!freopen("/dev/null", "r", stdin)) {
        perror("freopen stdin failed");
        return EXIT_FAILURE;
    }
    if (!freopen("bot.log", "a+", stdout)) {
        perror("freopen stdout failed");
        return EXIT_FAILURE;
    }
    if (!freopen("bot.log", "a+", stderr)) {
        perror("freopen stderr failed");
        return EXIT_FAILURE;
    }

    // Set the process group ID to the same as the process ID
    setpgid(0, 0);

    // Set environment variables
    setenv("TZ", "UTC", 1);

    // Run the bot in the background
    // Increment argv to skip the first element
    argv++;
    return execvp(argv[0], argv);  // Pass the modified argv to execvp
}
