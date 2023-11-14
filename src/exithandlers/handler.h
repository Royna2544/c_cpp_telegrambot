#pragma once

using exit_handler_t = void (*)(int);
using exit_handler_v_t = void (*)(void);

// OS specific
void _installExitHandler(void);
// Exported one
void installExitHandler(exit_handler_t fn);

extern exit_handler_t exitHandlerInt;
extern exit_handler_v_t exitHandlerVoid;
