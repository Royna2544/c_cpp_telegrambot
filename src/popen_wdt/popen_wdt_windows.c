#include <Windows.h>
#include <io.h>
#include <libos/libfs.h>
#include <locale.h>
#include <namedpipeapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "popen_wdt.h"

struct popen_wdt_windows_priv {
    HANDLE process_handle;
    HANDLE thread_handle;
    HANDLE child_stdout_r;
    HANDLE child_stdout_w_file;
    HANDLE exit_event;
    HANDLE rwProcessThread;
};

static HANDLE g_PrivMutex;

static DWORD WINAPI doReadWriteProcess(LPVOID arg) {
    popen_watchdog_data_t* data = (popen_watchdog_data_t*)arg;
    struct popen_wdt_windows_priv* pdata = data->privdata;
    DWORD readbuf = 0;
    static char buf[1024];

    ZeroMemory(&buf, sizeof(buf));
    for (;;) {
        WaitForSingleObject(g_PrivMutex, INFINITE);
        if (WaitForSingleObject(pdata->exit_event, 0) == WAIT_OBJECT_0) {
            break;
        }
        ReadFile(pdata->child_stdout_r, buf, sizeof(buf), &readbuf, NULL);
        WriteFile(pdata->child_stdout_w_file, buf, readbuf, NULL, NULL);
        ReleaseMutex(g_PrivMutex);
    }
    ReleaseMutex(g_PrivMutex);
    return 0;
}

static DWORD WINAPI watchdog(LPVOID arg) {
    popen_watchdog_data_t* data = (popen_watchdog_data_t*)arg;
    struct popen_wdt_windows_priv* pdata = data->privdata;
    DWORD ret_getexit = 0;
    DWORD exitEvent = 0;
    ULONGLONG startTime = GetTickCount64();

    WaitForSingleObject(g_PrivMutex, INFINITE);
    ResumeThread(pdata->thread_handle);
    pdata->exit_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (pdata->exit_event == NULL) {
        goto fail_exitevent;
    }
    pdata->rwProcessThread = CreateThread(
        NULL, 0, (LPTHREAD_START_ROUTINE)doReadWriteProcess, data, 0, NULL);
    if (pdata->rwProcessThread == NULL) {
        goto fail_rwph;
    }

    while (WaitForSingleObject(pdata->exit_event, 0) != WAIT_OBJECT_0) {
        if (GetExitCodeProcess(pdata->process_handle, &ret_getexit)) {
            if (ret_getexit != STILL_ACTIVE) {
                SetEvent(pdata->exit_event);
                WaitForSingleObject(pdata->rwProcessThread, INFINITE);
                data->watchdog_activated = false;
                break;
            } else {
                if (GetTickCount64() - startTime >
                    (ULONGLONG)(SLEEP_SECONDS * 1000)) {
                    SetEvent(pdata->exit_event);
                    WaitForSingleObject(pdata->rwProcessThread, INFINITE);
                    TerminateProcess(pdata->process_handle, 0);
                    data->watchdog_activated = true;
                    break;
                }
            }
        }
        Sleep(100);
    }
    CloseHandle(pdata->rwProcessThread);
fail_rwph:
    CloseHandle(pdata->exit_event);
fail_exitevent:
    CloseHandle(pdata->process_handle);
    CloseHandle(pdata->thread_handle);
    CloseHandle(pdata->child_stdout_r);
    CloseHandle(pdata->child_stdout_w_file);
    UnmapViewOfFile(pdata);
    if (data->fp != NULL) {
        fclose(data->fp);
    }
    free(data);
    ReleaseMutex(g_PrivMutex);
    return 0;
}

// [Child] stdout_w [pipe] stdout_r -> [Parent] stdout_r ->
// stdout_w_file [pipe] stdout_r_file [FILE*]
bool popen_watchdog_start(popen_watchdog_data_t* wdt_data) {
    HANDLE child_stdout_w = NULL, child_stdout_r = NULL;
    HANDLE child_stdout_w_file = NULL, child_stdout_r_file = NULL;
    HANDLE hMapFile = NULL;
    CHAR buffer[PATH_MAX] = {0};

    BOOL success = 0;
    SECURITY_ATTRIBUTES saAttr;
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    // Set up security attributes
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    // Create pipes, disable handle inherit
    if (!CreatePipe(&child_stdout_r, &child_stdout_w, &saAttr, 0)) {
        return false;
    }
    if (!SetHandleInformation(child_stdout_r, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(child_stdout_r);
        CloseHandle(child_stdout_w);
        return false;
    }

    if (wdt_data->watchdog_enabled) {
        // Create FILE* middleman pipe
        if (!CreatePipe(&child_stdout_r_file, &child_stdout_w_file, &saAttr,
                        0)) {
            CloseHandle(child_stdout_r);
            CloseHandle(child_stdout_w);
            return false;
        }
        // Alloc watchdog data
        // Create Mapping
        hMapFile =
            CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0,
                              sizeof(struct popen_wdt_windows_priv), NULL);
        if (hMapFile == NULL) {
            CloseHandle(child_stdout_r);
            CloseHandle(child_stdout_w);
            CloseHandle(child_stdout_r_file);
            CloseHandle(child_stdout_w_file);
            return false;
        }

        // Cast to watchdog privdata
        wdt_data->privdata = (struct popen_wdt_windows_priv*)MapViewOfFile(
            hMapFile, FILE_MAP_ALL_ACCESS, 0, 0,
            sizeof(struct popen_wdt_windows_priv));
        if (wdt_data->privdata == NULL) {
            CloseHandle(hMapFile);
            CloseHandle(child_stdout_r);
            CloseHandle(child_stdout_w);
            CloseHandle(child_stdout_r_file);
            CloseHandle(child_stdout_w_file);
            return false;
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

    // Try with default
    success = CreateProcess(NULL, (LPSTR)wdt_data->command, NULL, NULL, TRUE,
                            CREATE_NEW_PROCESS_GROUP | CREATE_SUSPENDED, NULL,
                            NULL, &si, &pi);

    if (!success) {
        // Hmm? We failed? Try to append powershell -c
        // Create command line string
        snprintf(buffer, sizeof(buffer), "powershell.exe -c \"%s\"",
                 wdt_data->command);

        // Create process again
        success = CreateProcess(NULL, (LPSTR)buffer, NULL, NULL, TRUE,
                                CREATE_NEW_PROCESS_GROUP | CREATE_SUSPENDED,
                                NULL, NULL, &si, &pi);
    }
    if (!success) {
        // Still no? abort.
        if (wdt_data->watchdog_enabled) {
            CloseHandle(hMapFile);
            CloseHandle(child_stdout_r_file);
            CloseHandle(child_stdout_w_file);
        }
        CloseHandle(child_stdout_r);
        CloseHandle(child_stdout_w);
        return false;
    }

    // Cleanup
    // Close Handles
    CloseHandle(child_stdout_w);
    if (wdt_data->watchdog_enabled) {
        struct popen_wdt_windows_priv pdata;
        pdata.process_handle = pi.hProcess;
        pdata.thread_handle = pi.hThread;
        pdata.child_stdout_r = child_stdout_r;
        pdata.child_stdout_w_file = child_stdout_w_file;

        memcpy(wdt_data->privdata, &pdata,
               sizeof(struct popen_wdt_windows_priv));
        HANDLE wdtThHdl = CreateThread(NULL, 0, watchdog, wdt_data, 0, NULL);
        if (wdtThHdl != NULL) {
            CloseHandle(wdtThHdl);
            wdt_data->fp =
                _fdopen(_open_osfhandle((intptr_t)child_stdout_r_file, 0), "r");
        }
    } else {
        CloseHandle(pi.hProcess);
        ResumeThread(pi.hThread);
        CloseHandle(pi.hThread);
        wdt_data->fp =
            _fdopen(_open_osfhandle((intptr_t)child_stdout_r, 0), "r");
    }
    return true;
}

void popen_watchdog_stop(popen_watchdog_data_t* data) {
    const struct popen_wdt_windows_priv* pdata = data->privdata;
    WaitForSingleObject(g_PrivMutex, INFINITE);
    if (data->fp != NULL) {
        fclose(data->fp);
    }
    if (data->watchdog_enabled) {
        data->watchdog_activated = false;
        CloseHandle(pdata->rwProcessThread);
        CloseHandle(pdata->process_handle);
        CloseHandle(pdata->thread_handle);
        CloseHandle(pdata->child_stdout_w_file);
        CloseHandle(pdata->exit_event);
        UnmapViewOfFile(pdata);
    }
    free(data);
    ReleaseMutex(g_PrivMutex);
}
