#pragma once

#include <stdbool.h>
#include <stdio.h>

#ifndef NDEBUG
#define POPEN_WDT_DEBUG
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Identical to POSIX SIGINT, what is currently used to terminate the process
#define POPEN_WDT_SIGTERM 2

typedef struct {
    int exitcode; /* exit code of the process, default -255 */
    bool signal;  /* if the process was signaled, then the exit code becomes the
                     signum */
} popen_watchdog_exit_t;

typedef struct {
    const char *command;     /* command string */
    bool watchdog_enabled;   /* Is watchdog enabled? [in] */
    bool watchdog_activated; /* Result callback, stored true if watchdog did the
                                work [out] */
    int sleep_secs;          /* Number of seconds to sleep if watchdog is enabled */
    void *privdata;          /* Private data pointer */
} popen_watchdog_data_t;

/**
 * @brief initializes the popen watchdog data structure
 *
 * @param data the data structure to initialize
 * @return if the initialization succeeded
 */
bool popen_watchdog_init(popen_watchdog_data_t **data);

/**
 * @brief starts the popen watchdog, which monitors the given command and kills
 * it if it hangs
 *
 * @param data the data structure containing the command to monitor
 * @return true if the watchdog was successfully started, false otherwise
 */
bool popen_watchdog_start(popen_watchdog_data_t **data);

/**
 * @brief stops the popen watchdog, which monitors the given command and kills
 * it if it hangs
 *
 * @param data the data structure containing the command to monitor
 */
void popen_watchdog_stop(popen_watchdog_data_t **data);

/**
 * @brief Checks if the watchdog has been activated for the given popen data.
 *
 * @param data The data structure containing the popen information.
 * @return true if the watchdog activated, false otherwise.
 */
bool popen_watchdog_activated(popen_watchdog_data_t **data);

/**
 * @brief Reads data from the file pointer associated with the popen watchdog.
 *
 * This function reads up to 'size' bytes from the file pointer associated with
 * the popen watchdog and stores the data in the provided buffer.
 *
 * @param data A double pointer to the popen watchdog data.
 * @param buf A pointer to the buffer where the read data will be stored.
 * @param size The maximum number of bytes to read from the file pointer.
 * @return true if data was successfully read, false otherwise.
 */
bool popen_watchdog_read(popen_watchdog_data_t **data, char *buf, int size);

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
popen_watchdog_exit_t popen_watchdog_destroy(popen_watchdog_data_t **data);

#ifdef __cplusplus
}
#endif