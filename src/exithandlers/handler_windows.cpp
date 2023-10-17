#include "handler.h"

#include <windows.h>

static exit_handler_t fn;

static BOOL WINAPI CtrlHandler(DWORD fdwCtrlType) {
    switch (fdwCtrlType) {
        case CTRL_C_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            if (fn)
                fn(0);
        default:
            return FALSE;
    }
}

void installExitHandler(exit_handler_t fn_) {
    fn = fn_;
    SetConsoleCtrlHandler(CtrlHandler, TRUE);
}
