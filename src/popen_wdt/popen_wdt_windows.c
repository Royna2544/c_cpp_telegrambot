#include <Windows.h>
#include <io.h>
#include <stdio.h>
#include <pthread.h>

#include "popen_wdt.h"

struct watchdog_data {
    HANDLE process_handle;
    DWORD process_id;
    HANDLE pipefd[2];
};

static void* watchdog(void* arg) {
    struct watchdog_data* data = (struct watchdog_data*)arg;
    DWORD ret_getexit = 0;
    Sleep(SLEEP_SECONDS);
    if (GetExitCodeProcess(data->process_handle, &ret_getexit)) {
        if (ret_getexit == STILL_ACTIVE) {
            const HANDLE explorer = OpenProcess(PROCESS_TERMINATE, false, data->process_id);
            TerminateProcess(explorer, 1);
            CloseHandle(explorer);
        }
    }
    CloseHandle(data->pipefd[0]);
    CloseHandle(data->pipefd[1]);
    UnmapViewOfFile(data);
    return 0;
}

FILE* popen_watchdog(const char* command, const bool wdt_on) {
    FILE* fp;
    HANDLE pipeHandles[2] = {NULL, NULL};
    HANDLE pid;
    struct watchdog_data* data = NULL;
    CHAR *command_full, dummy[PATH_MAX];
    int ret;

    if (!CreatePipe(&pipeHandles[0], &pipeHandles[1], NULL, 0)) {
        return NULL;
    }

    HANDLE hMapFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(struct watchdog_data), NULL);
    if (hMapFile == NULL)
        return NULL;
    data = (struct watchdog_data*)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0,
        sizeof(struct watchdog_data));
    if (data == NULL) {
        CloseHandle(hMapFile);
        return NULL;
    }

    STARTUPINFO si = {0};
    si.cb = sizeof(STARTUPINFO);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = pipeHandles[1];
    si.hStdError = pipeHandles[1];

    PROCESS_INFORMATION pi = {0};
    // TODO: Hardcoded powershell
    #define PowerShellFmt "C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe -c \"%s\""
    ret = sprintf(dummy, PowerShellFmt, command);
    command_full = (CHAR *)malloc(ret + 1);
    snprintf(command_full, ret + 1, PowerShellFmt, command);
    BOOL success = CreateProcess(NULL, (LPSTR)command_full, NULL, NULL, TRUE,
        CREATE_NEW_PROCESS_GROUP | DETACHED_PROCESS, NULL, NULL, &si, &pi);
    if (!success) {
        CloseHandle(pipeHandles[0]);
        CloseHandle(pipeHandles[1]);
        return NULL;
    }
    free(command_full);
    data->process_handle = pi.hProcess;
    data->process_id = pi.dwProcessId;
    data->pipefd[0] = pipeHandles[0];
    data->pipefd[1] = pipeHandles[1];

    pthread_t watchdog_thread;
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&watchdog_thread, &attr, &watchdog, data);
    pthread_attr_destroy(&attr);

    fp = _fdopen(_open_osfhandle((intptr_t)pipeHandles[0], 0), "r");

    return fp;
}