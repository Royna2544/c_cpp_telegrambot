#include <Windows.h>
#include <io.h>
#include <pthread.h>
#include <stdio.h>
#include <utils/libutils.h>

#include "popen_wdt.h"

struct watchdog_data {
    HANDLE process_handle;
    HANDLE thread_handle;
    HANDLE child_stdout_r;
    HANDLE child_stdout_w_file;
};

static void* watchdog(void* arg) {
    struct watchdog_data* data = (struct watchdog_data*)arg;
    DWORD ret_getexit = 0;
    DWORD startTime = GetTickCount();
    bool dead = false;

    ResumeThread(data->thread_handle);
    for (;;) {
        DWORD readbuf;
        static char buf[1024];
        if (GetExitCodeProcess(data->process_handle, &ret_getexit)) {
            if (ret_getexit != STILL_ACTIVE) {
                dead = true;
                break;
            }
        }
        ZeroMemory(&buf, sizeof(buf));
        ReadFile(data->child_stdout_r, buf, sizeof(buf), &readbuf, NULL);
        WriteFile(data->child_stdout_w_file, buf, readbuf, NULL, NULL);
        if (GetTickCount() - startTime > SLEEP_SECONDS * 1000) {
            // Give time for exit
            Sleep(1);
            break;
        }
    }
    if (!dead && GetExitCodeProcess(data->process_handle, &ret_getexit)) {
        if (ret_getexit == STILL_ACTIVE) {
            TerminateProcess(data->process_handle, 0);
        }
    }
    CloseHandle(data->process_handle);
    CloseHandle(data->thread_handle);
    CloseHandle(data->child_stdout_r);
    CloseHandle(data->child_stdout_w_file);
    UnmapViewOfFile(data);
    return 0;
}

// [Child] stdout_w [pipe] stdout_r -> [Parent] stdout_r ->
// stdout_w_file [pipe] stdout_r_file [FILE*]
FILE* popen_watchdog(const char* command, const bool wdt_on) {
    FILE* fp;
    HANDLE child_stdout_w = NULL, child_stdout_r = NULL;
    HANDLE child_stdout_w_file = NULL, child_stdout_r_file = NULL;
    HANDLE pid = NULL;

    struct watchdog_data* data = NULL;
    CHAR *command_full, dummy[PATH_MAX];
    int ret;

    BOOL success;
    SECURITY_ATTRIBUTES saAttr;
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    // Set up security attributes
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    // Create pips, disable handle inherit
    if (!CreatePipe(&child_stdout_r, &child_stdout_w, &saAttr, 0)) {
        return NULL;
    }
    if (!CreatePipe(&child_stdout_r_file, &child_stdout_w_file, &saAttr, 0)) {
        return NULL;
    }
    if (!SetHandleInformation(child_stdout_r, HANDLE_FLAG_INHERIT, 0)) {
        return NULL;
    }

    // Alloc watchdog data
    // Create Mapping
    HANDLE hMapFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0,
                                        sizeof(struct watchdog_data), NULL);
    if (hMapFile == NULL)
        return NULL;

    // Cast to watchdog privdata
    data = (struct watchdog_data*)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0,
                                                sizeof(struct watchdog_data));
    if (data == NULL) {
        CloseHandle(hMapFile);
        return NULL;
    }

    // Init memory
    ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&si, sizeof(STARTUPINFO));

    // Setup processinfo
    si.cb = sizeof(STARTUPINFO);
    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdOutput = child_stdout_w;
    si.hStdError = child_stdout_w;

    // Create command line string
    #define PowerShellFmt "powershell.exe -c \"%s\""
    ret = snprintf(dummy, sizeof(dummy), PowerShellFmt, command);
    command_full = (CHAR*)malloc(ret + 1);
    snprintf(command_full, ret + 1, PowerShellFmt, command);
    PRETTYF("Command: %s", command_full);

    // Create process
    success = CreateProcess(NULL, (LPSTR)command_full, NULL, NULL, TRUE,
                            CREATE_NEW_PROCESS_GROUP | CREATE_SUSPENDED, NULL, NULL, &si, &pi);
    if (!success) {
        CloseHandle(hMapFile);
        CloseHandle(child_stdout_w);
        return NULL;
    }

    // Cleanup
    // Free memory
    free(command_full);

    // Close Handles
    CloseHandle(child_stdout_w);
    data->process_handle = pi.hProcess;
    data->thread_handle = pi.hThread;
    data->child_stdout_r = child_stdout_r;
    data->child_stdout_w_file = child_stdout_w_file;

    // Launch thread
    pthread_t watchdog_thread;
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&watchdog_thread, &attr, &watchdog, data);
    pthread_attr_destroy(&attr);

    fp = _fdopen(_open_osfhandle((intptr_t)child_stdout_r_file, 0), "r");

    return fp;
}