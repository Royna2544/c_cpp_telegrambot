#include <Windows.h>
#include <io.h>
#include <locale.h>
#include <pthread.h>
#include <stdio.h>
#include <utils/libutils.h>

#include "popen_wdt.h"

struct watchdog_data {
    HANDLE process_handle;
    HANDLE thread_handle;
    HANDLE child_stdout_r;
    HANDLE child_stdout_w_file;
    bool* result_cb;
};

static DWORD WINAPI doReadWriteProcess(LPVOID arg) {
    DWORD readbuf, result;
    struct watchdog_data* data = (struct watchdog_data*)arg;
    static char buf[1024];

    ZeroMemory(&buf, sizeof(buf));
    for (;;) {
        ReadFile(data->child_stdout_r, buf, sizeof(buf), &readbuf, NULL);
        WriteFile(data->child_stdout_w_file, buf, readbuf, NULL, NULL);
    }
    return 0;
}

static void* watchdog(void* arg) {
    struct watchdog_data* data = (struct watchdog_data*)arg;
    HANDLE rwProcessThread = NULL;
    DWORD ret_getexit = 0;
    DWORD startTime = GetTickCount();

    ResumeThread(data->thread_handle);
    rwProcessThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)doReadWriteProcess,
                                   data, 0, NULL);
    for (;;) {
        if (GetExitCodeProcess(data->process_handle, &ret_getexit)) {
            if (ret_getexit != STILL_ACTIVE) {
                TerminateThread(rwProcessThread, 0);
                *data->result_cb = false;
                break;
            } else {
                if (GetTickCount() - startTime > SLEEP_SECONDS * 1000) {
                    TerminateThread(rwProcessThread, 0);
                    TerminateProcess(data->process_handle, 0);
                    *data->result_cb = true;
                    break;
                }
            }
        }
    }
    CloseHandle(rwProcessThread);
    CloseHandle(data->process_handle);
    CloseHandle(data->thread_handle);
    CloseHandle(data->child_stdout_r);
    CloseHandle(data->child_stdout_w_file);
    UnmapViewOfFile(data);
    return 0;
}
void setlocale_enus_once(void) {
    SetThreadUILanguage(/* en-US */ 0x0409);
}
// [Child] stdout_w [pipe] stdout_r -> [Parent] stdout_r ->
// stdout_w_file [pipe] stdout_r_file [FILE*]
FILE* popen_watchdog(const char* command, bool* wdt_ret) {
    FILE* fp;
    HANDLE child_stdout_w = NULL, child_stdout_r = NULL;
    HANDLE child_stdout_w_file = NULL, child_stdout_r_file = NULL;
    HANDLE pid = NULL;
    HANDLE hMapFile = NULL;

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
    if (wdt_ret) {
        if (!CreatePipe(&child_stdout_r_file, &child_stdout_w_file, &saAttr, 0)) {
            return NULL;
        }
    }
    if (!SetHandleInformation(child_stdout_r, HANDLE_FLAG_INHERIT, 0)) {
        return NULL;
    }

    if (wdt_ret) {
        // Alloc watchdog data
        // Create Mapping
        hMapFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0,
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
        if (wdt_ret) {
            CloseHandle(hMapFile);
            CloseHandle(child_stdout_r_file);
            CloseHandle(child_stdout_w_file);
        }
        CloseHandle(child_stdout_r);
        CloseHandle(child_stdout_w);
        return NULL;
    }

    // Cleanup
    // Free memory
    free(command_full);

    // Close Handles
    CloseHandle(child_stdout_w);
    if (wdt_ret) {
        data->result_cb = wdt_ret;
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
    } else {
        CloseHandle(pi.hProcess);
        ResumeThread(pi.hThread);
        CloseHandle(pi.hThread);
        fp = _fdopen(_open_osfhandle((intptr_t)child_stdout_r, 0), "r");
    }

    return fp;
}