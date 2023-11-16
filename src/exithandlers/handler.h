#pragma once

using exit_handler_t = void (*)(int);
using exit_handler_v_t = void (*)(void);

/**
 * _installExitHandler - install exit handler (Internal)
 * Exit handler is the function that is called after exit or signal
 * Object that is installed is the global exitHandlerInt, exitHandlerVoid
 * which is set in installExitHandler
 *
 * Platform-specific.
 */
void _installExitHandler(void);

/**
 * installExitHandler - install exit handler
 *
 * Install exit handler
 * @param fn exit_handler_t object, that is called on exit or signal receive
 */
void installExitHandler(exit_handler_t fn);

extern exit_handler_t exitHandlerInt;
extern exit_handler_v_t exitHandlerVoid;
