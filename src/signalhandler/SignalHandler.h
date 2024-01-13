#pragma once

using exit_handler_t = void (*)(int);

/**
 * installSignalHandler - install signal handler
 * Signal handler is the function that is called after signal event
 *
 * Platform-specific.
 *
 * @param fn exit_handler_t object, that is called on signal receive
 */
void installSignalHandler(exit_handler_t fn);
