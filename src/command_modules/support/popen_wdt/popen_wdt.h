#pragma once

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#ifndef _WIN32
#include <signal.h>
#endif

#ifndef NDEBUG
#define POPEN_WDT_DEBUG
#endif

// Exit code types
#ifdef _WIN32
typedef unsigned long popen_watchdog_exit_code_t;
#define POPEN_WDT_EXIT_CODE_MAX ULONG_MAX
#else
typedef uint8_t popen_watchdog_exit_code_t;
#define POPEN_WDT_EXIT_CODE_MAX UINT8_MAX
#endif

// Default shell
#ifdef _WIN32
#define POPEN_WDT_DEFAULT_SHELL "powershell.exe"
#elif __APPLE__
#define POPEN_WDT_DEFAULT_SHELL "zsh"
#elif defined __linux__
#define POPEN_WDT_DEFAULT_SHELL "bash"
#elif defined __FreeBSD__ || defined __ANDROID__
#define POPEN_WDT_DEFAULT_SHELL "sh"
#else
#error "Unsupported platform"
#endif

// Default sleep seconds if not specified.
#define POPEN_WDT_DEFAULT_SLEEP_SECS 10

// Status values used when a watchdog has to stop a process. Keep the Windows
// termination code stable while exposing the real POSIX signal numbers on
// Unix-like hosts.
#ifdef _WIN32
#define POPEN_WDT_SIGTERM 2
#define POPEN_WDT_SIGKILL 9
#else
#define POPEN_WDT_SIGTERM SIGTERM
#define POPEN_WDT_SIGKILL SIGKILL
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    popen_watchdog_exit_code_t exitcode; /* exit code of the process, default
                                            POPEN_WDT_EXIT_CODE_MAX */
    bool signal; /* if the process was signaled, if yes, exitcode is signal num
                  */
} popen_watchdog_exit_t;

#define POPEN_WDT_EXIT_INITIALIZER {POPEN_WDT_EXIT_CODE_MAX, false}

typedef struct {
    const char* command;     /* command string */
    bool watchdog_enabled;   /* Is watchdog enabled? [in] */
    bool watchdog_activated; /* True after timeout or explicit cancellation */
    int sleep_secs; /* Number of seconds to sleep if watchdog is enabled */
    void* privdata; /* Private data pointer */
} popen_watchdog_data_t;

typedef int64_t popen_watchdog_ssize_t;

/**
 * @brief initializes the popen watchdog data structure
 *
 * @param data the data structure to initialize
 * @return if the initialization succeeded
 */
bool popen_watchdog_init(popen_watchdog_data_t** data);

/**
 * @brief starts the popen watchdog, which monitors the given command and kills
 * it if it hangs
 *
 * @param data the data structure containing the command to monitor
 * @return true if the watchdog was successfully started, false otherwise
 */
bool popen_watchdog_start(popen_watchdog_data_t** data);

/**
 * @brief Checks if the watchdog has been activated for the given popen data.
 *
 * @param data The data structure containing the popen information.
 * @return true if the watchdog activated, false otherwise.
 */
bool popen_watchdog_activated(popen_watchdog_data_t** data);

/**
 * @brief Cancels the running command and its process group.
 *
 * POSIX
 * callers get the same graceful shutdown as a watchdog timeout.
 * SIGTERM is
 * sent to the whole process group, followed by SIGKILL after a
 * two-second
 * grace period when any member remains. The call is synchronous so
 * the owner
 * may safely proceed to destroy the watchdog after it returns.
 *
 * @return
 * true when cancellation was requested for a running process.
 */
bool popen_watchdog_cancel(popen_watchdog_data_t** data);

/**
 * @brief Reads data from the file pointer associated with the popen watchdog.
 *
 * This function reads up to 'size' bytes from the file pointer associated with
 * the popen watchdog and stores the data in the provided buffer.
 *
 * @param data A double pointer to the popen watchdog data.
 * @param buf A pointer to the buffer where the read data will be stored.
 * @param size The maximum number of bytes to read from the file pointer.
 * @return Total size of read bytes, fail means negative.
 */
popen_watchdog_ssize_t popen_watchdog_read(popen_watchdog_data_t** data,
                                           char* buf,
                                           popen_watchdog_ssize_t size);

/**
 * @brief Cleans up and frees the resources associated with the popen watchdog
 * data.
 *
 * This function should be called when the popen watchdog data is no longer
 * needed. It closes the file pointer, frees the memory allocated for the data
 * structure, and sets the data pointer to NULL.
 *
 * @param data A double pointer to the popen watchdog data. The function will
 * set this pointer to NULL after cleaning up the resources.
 * @return The exit status of the process. If the process was signaled, the
 * exit status will be the signum.
 */
popen_watchdog_exit_t popen_watchdog_destroy(popen_watchdog_data_t** data);

#ifdef __cplusplus
}
#endif
