#include <windows.h>

#include "libsighandler.hpp"

BOOL WINAPI CtrlHandler(DWORD fdwCtrlType) {
    switch (fdwCtrlType) {
        case CTRL_C_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            SignalHandler::signalHandler();
        default:
            return FALSE;
    }
}

void SignalHandler::install() {
    SetConsoleCtrlHandler(CtrlHandler, TRUE);
}

void SignalHandler::uninstall() {
    SetConsoleCtrlHandler(CtrlHandler, FALSE);
}
