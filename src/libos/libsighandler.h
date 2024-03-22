#pragma once

using exit_handler_t = void (*)(int);

/**
 * installSignalHandler - install the default signal handler
 * Signal handler is the function that is called after signal event
 *
 * Platform-specific.
 *
 */
void installSignalHandler();

/**
 * defaultSignalHandler - default signal handler
 *
 * @param sig signal number
 */
extern void defaultSignalHandler(int sig);
extern void defaultCleanupFunction();

constexpr int invalidSignal = -1;