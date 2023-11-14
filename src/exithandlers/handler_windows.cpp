#include <windows.h>

#include "handler.h"

static BOOL WINAPI CtrlHandler(DWORD fdwCtrlType) {
    switch (fdwCtrlType) {
        case CTRL_C_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            exitHandlerVoid();
        default:
            return FALSE;
    }
}

void _installExitHandler(void) {
    SetConsoleCtrlHandler(CtrlHandler, TRUE);
}
