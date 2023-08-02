#include <Windows.h>
#include <io.h>
#include <stdio.h>

#include "popen_wdt.h"

struct watchdog_data {
    HANDLE process_handle;
    DWORD process_id;
    HANDLE pipe_ret;
};

static DWORD WINAPI watchdog(LPVOID arg) {
    struct watchdog_data* data = (struct watchdog_data*)arg;
    DWORD ret = 0, ret_getexit = 0;
    Sleep(SLEEP_SECONDS);
    if (GetExitCodeProcess(data->process_handle, &ret_getexit) && ret_getexit == STILL_ACTIVE) {
        GenerateConsoleCtrlEvent(CTRL_C_EVENT, data->process_id);
        ret = 1;
    }
    if (!ret) {
        // The caller may have exited in this case, we shouldn't hang again
        unblockForHandle(data->pipe_ret);
    }
    if (data->pipe_ret != INVALID_HANDLE_VALUE)
        WriteFile(data->pipe_ret, &ret, sizeof(DWORD), NULL, NULL);
    UnmapViewOfFile(data);
    return 0;
}

FILE* popen_watchdog(const char* command, const POPEN_WDT_HANDLE pipe_ret) {
    FILE* fp;
    HANDLE pipeHandles[2] = {NULL, NULL};
    HANDLE pid;
    struct watchdog_data* data = NULL;
    int watchdog_on = pipe_ret != INVALID_HANDLE_VALUE, ret;
    CHAR command_full[PATH_MAX];

    if (!InitPipeHandle(&pipeHandles)) {
        return NULL;
    }

    if (watchdog_on) {
        HANDLE hMapFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(struct watchdog_data), NULL);
        if (hMapFile == NULL)
            return NULL;

        data = (struct watchdog_data*)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(struct watchdog_data));
        if (data == NULL) {
            CloseHandle(hMapFile);
            return NULL;
        }

        data->pipe_ret = pipe_ret;

        HANDLE watchdog_thread = CreateThread(NULL, 0, watchdog, data, 0, NULL);
        if (watchdog_thread == NULL) {
            CloseHandle(hMapFile);
            return NULL;
        }
        CloseHandle(watchdog_thread);
    }

    STARTUPINFO si = {0};
    si.cb = sizeof(STARTUPINFO);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = pipeHandles[1];
    si.hStdError = pipeHandles[1];

    PROCESS_INFORMATION pi = {0};
    // TODO: Hardcoded powershell
    ret = snprintf(command_full, sizeof(command_full), 
        "C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe -c \"%s\"", command);
    if (command_full[ret - 1] != '"') {
        // Truncated
        return NULL;
    }
    BOOL success = CreateProcess(NULL, (LPSTR)command_full, NULL, NULL, TRUE,
        CREATE_NEW_PROCESS_GROUP, NULL, NULL, &si, &pi);
    if (!success) {
        CloseHandle(pipeHandles[0]);
        CloseHandle(pipeHandles[1]);
        return NULL;
    }

    if (watchdog_on) {
        data->process_handle = pi.hProcess;
        data->process_id = pi.dwProcessId;
    }

    CloseHandle(pipeHandles[1]);
    CloseHandle(pi.hThread);
    fp = _fdopen(_open_osfhandle((intptr_t)pipeHandles[0], 0), "r");

    return fp;
}

void unblockForHandle(const POPEN_WDT_HANDLE fd) {
    DWORD mode;

    if (fd == INVALID_HANDLE_VALUE) return;
    GetNamedPipeHandleState(fd, &mode, NULL, NULL, NULL, NULL, 0);
    SetNamedPipeHandleState(fd, &mode, NULL, NULL);
}

bool InitPipeHandle(POPEN_WDT_HANDLE (*fd)[2]) {
    if (!fd) return false;
    (*fd)[0] = invalid_fd_value;
    (*fd)[1] = invalid_fd_value;
    return CreatePipe(fd[0], fd[1], NULL, 0);
}

void writeBoolToHandle(const POPEN_WDT_HANDLE fd, bool value) {
    WriteFile(fd, &value, sizeof(value), NULL, NULL);
}

bool readBoolFromHandle(const POPEN_WDT_HANDLE fd) {
    bool ret = false;
    ReadFile(fd, &ret, sizeof(ret), NULL, NULL);
    return ret;
}

void closeHandle(const POPEN_WDT_HANDLE fd) {
    CloseHandle(fd);
}

POPEN_WDT_HANDLE invalid_fd_value = INVALID_HANDLE_VALUE;