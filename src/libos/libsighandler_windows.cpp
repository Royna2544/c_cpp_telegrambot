#include <windows.h>

#include "libsighandler.h"

static BOOL WINAPI CtrlHandler(DWORD fdwCtrlType) {
    switch (fdwCtrlType) {
        case CTRL_C_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            defaultSignalHandler(invalidSignal);
        default:
            return FALSE;
    }
}

void installSignalHandler() {
    SetConsoleCtrlHandler(CtrlHandler, TRUE);
}
