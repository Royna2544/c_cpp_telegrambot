#include <windows.h>

#include "SignalHandler.h"

static exit_handler_t exitHandler = nullptr;

static BOOL WINAPI CtrlHandler(DWORD fdwCtrlType) {
    switch (fdwCtrlType) {
        case CTRL_C_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            exitHandler(-1);
        default:
            return FALSE;
    }
}

void installSignalHandler(const exit_handler_t et) {
    exitHandler = et;
    SetConsoleCtrlHandler(CtrlHandler, TRUE);
}
