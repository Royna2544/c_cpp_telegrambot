#include <windows.h>

static void (*cleanupFn)() = NULL;

static BOOL WINAPI CtrlHandler(DWORD fdwCtrlType) {
    switch (fdwCtrlType) {
        case CTRL_C_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            if (cleanupFn)
                cleanupFn();
            return FALSE;

        default:
            return FALSE;
    }
}

BOOL installHandler(void (*cleanupFn_)()) {
    cleanupFn = cleanupFn_;
    return SetConsoleCtrlHandler(CtrlHandler, TRUE);
}
