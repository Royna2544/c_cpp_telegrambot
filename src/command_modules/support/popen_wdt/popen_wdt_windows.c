#include <Windows.h>
#include <handleapi.h>
#include <locale.h>
#include <memoryapi.h>
#include <minwinbase.h>
#include <namedpipeapi.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <synchapi.h>
#include <winbase.h>
#include <winnt.h>

#include "popen_wdt.h"

#ifdef POPEN_WDT_DEBUG
#define POPEN_WDT_DBGLOG(fmt, ...)                                        \
    do {                                                                  \
        printf("POPEN_WDT::WIN32: Func %s, Line %d: " fmt "\n", __func__, \
               __LINE__, ##__VA_ARGS__);                                  \
    } while (0)
#else
#define POPEN_WDT_DBGLOG(fmt, ...)
#endif
#define POPEN_WDT_PIPE "\\\\.\\pipe\\popen_wdt"

struct popen_wdt_windows_priv {
    struct {
        HANDLE sub_Process;
        HANDLE sub_Thread;
        HANDLE thread;
    } wdt_data;
    HANDLE read_hdl;
    HANDLE write_hdl;
};

static bool check_data_privdata(popen_watchdog_data_t** data) {
    if (data == NULL) {
        POPEN_WDT_DBGLOG("data is NULL");
        return false;
    }
    if (*data == NULL) {
        POPEN_WDT_DBGLOG("data points to NULL");
        return false;
    }
    if ((*data)->privdata == NULL) {
        POPEN_WDT_DBGLOG("data->privdata is NULL");
        return false;
    }
    return true;
}

static DWORD WINAPI watchdog(LPVOID arg) {
    popen_watchdog_data_t** data = (popen_watchdog_data_t**)arg;
    struct popen_wdt_windows_priv* pdata = (*data)->privdata;
    DWORD ret_getexit = 0;
    DWORD exitEvent = 0;
    const ULONGLONG endTime = GetTickCount64() + (*data)->sleep_secs * 1000ULL;

    ResumeThread(pdata->wdt_data.sub_Thread);

    while (true) {
        if (!GetExitCodeThread(pdata->wdt_data.sub_Thread, &ret_getexit)) {
            POPEN_WDT_DBGLOG("Failed to get exit code");
            return false;
        }
        if (ret_getexit != STILL_ACTIVE) {
            POPEN_WDT_DBGLOG("Subprocess exited");
            break;
        } else if (GetTickCount64() > endTime) {
            POPEN_WDT_DBGLOG("Beginning watchdog trigger event");
            POPEN_WDT_DBGLOG("Now terminating subprocess");
            TerminateProcess(pdata->wdt_data.sub_Process, POPEN_WDT_SIGTERM);
            POPEN_WDT_DBGLOG("... done");
            (*data)->watchdog_activated = true;
            break;
        }
        Sleep(100);
    }
    POPEN_WDT_DBGLOG("Done");
    char buf = 0;
    WriteFile(pdata->write_hdl, &buf, sizeof(char), NULL, NULL);
    CloseHandle(pdata->write_hdl);
    pdata->write_hdl = NULL;

    return 0;
}

bool popen_watchdog_start(popen_watchdog_data_t** wdt_data_in) {
    HANDLE child_stdout_w = NULL;
    HANDLE child_stdout_r = NULL;
    HANDLE hMapFile = NULL;
    popen_watchdog_data_t* wdt_data = NULL;
    struct popen_wdt_windows_priv pdata;

    CHAR buffer[MAX_PATH] = {0};
    BOOL success = 0;
    SECURITY_ATTRIBUTES saAttr;
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    if (wdt_data_in == NULL) {
        POPEN_WDT_DBGLOG("wdt_data_in is NULL");
        return false;
    }
    if (*wdt_data_in == NULL) {
        POPEN_WDT_DBGLOG("wdt_data_in is NULLPTR");
        return false;
    }
    wdt_data = *wdt_data_in;

    // Set up security attributes
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    if (wdt_data->watchdog_enabled) {
        // Alloc watchdog data
        // Create Mapping
        hMapFile =
            CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0,
                              sizeof(struct popen_wdt_windows_priv), NULL);
        if (hMapFile == NULL) {
            return false;
        }

        // Cast to watchdog privdata
        wdt_data->privdata = (struct popen_wdt_windows_priv*)MapViewOfFile(
            hMapFile, FILE_MAP_ALL_ACCESS, 0, 0,
            sizeof(struct popen_wdt_windows_priv));
        if (wdt_data->privdata == NULL) {
            CloseHandle(hMapFile);
            return false;
        }
    } else {
        // Malloc does the work here, no need to be shared among threads
        wdt_data->privdata = malloc(sizeof(struct popen_wdt_windows_priv));
        if (wdt_data->privdata == NULL) {
            return false;
        }
    }

    // memset memory
    ZeroMemory(wdt_data->privdata, sizeof(struct popen_wdt_windows_priv));
    ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&si, sizeof(STARTUPINFO));

    // Setup processinfo
    si.cb = sizeof(STARTUPINFO);
    si.dwFlags |= STARTF_USESTDHANDLES;
    child_stdout_w = si.hStdError = si.hStdOutput = CreateNamedPipeA(
        POPEN_WDT_PIPE, PIPE_ACCESS_OUTBOUND | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE | PIPE_WAIT, 1, 0, 0, 0, &saAttr);
    if (child_stdout_w == INVALID_HANDLE_VALUE) {
        POPEN_WDT_DBGLOG("CreateNamedPipe failed with error %lu",
                         GetLastError());
        if (wdt_data->watchdog_enabled) {
            UnmapViewOfFile(wdt_data->privdata);
            CloseHandle(hMapFile);
        } else {
            free(wdt_data->privdata);
        }
        wdt_data->privdata = NULL;
        return false;
    }
    child_stdout_r = CreateFileA(POPEN_WDT_PIPE, GENERIC_READ, 0, NULL,
                                 OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    if (child_stdout_r == INVALID_HANDLE_VALUE) {
        POPEN_WDT_DBGLOG("CreateFile failed with error %lu", GetLastError());
        if (wdt_data->watchdog_enabled) {
            UnmapViewOfFile(wdt_data->privdata);
            CloseHandle(hMapFile);
        } else {
            free(wdt_data->privdata);
        }
        wdt_data->privdata = NULL;
        DisconnectNamedPipe(child_stdout_w);
        CloseHandle(child_stdout_w);
        return false;
    }

    setlocale(LC_ALL, "C");

    // Try with default
    success = CreateProcessA(NULL, (LPSTR)wdt_data->command, NULL, NULL, TRUE,
                             CREATE_NEW_PROCESS_GROUP | CREATE_SUSPENDED, NULL,
                             NULL, &si, &pi);
    POPEN_WDT_DBGLOG("Command is: %s", wdt_data->command);

    if (!success) {
        // Hmm? We failed? Try to append powershell -c
        // Create command line string
        (void)snprintf(buffer, sizeof(buffer), "powershell.exe -c \"%s\"",
                       wdt_data->command);
        POPEN_WDT_DBGLOG("New command is: %s", buffer);

        // Create process again
        success = CreateProcessA(NULL, buffer, NULL, NULL, TRUE,
                                 CREATE_NEW_PROCESS_GROUP | CREATE_SUSPENDED,
                                 NULL, NULL, &si, &pi);
    }
    if (!success) {
        // Still no? abort.
        if (wdt_data->watchdog_enabled) {
            UnmapViewOfFile(wdt_data->privdata);
            CloseHandle(hMapFile);
        } else {
            free(wdt_data->privdata);
        }
        wdt_data->privdata = NULL;
        CloseHandle(child_stdout_r);
        DisconnectNamedPipe(child_stdout_w);
        CloseHandle(child_stdout_w);
        return false;
    }

    pdata.read_hdl = child_stdout_r;
    pdata.write_hdl = child_stdout_w;
    if (wdt_data->watchdog_enabled) {
        pdata.wdt_data.sub_Process = pi.hProcess;
        pdata.wdt_data.sub_Thread = pi.hThread;

        pdata.wdt_data.thread =
            CreateThread(NULL, 0, watchdog, wdt_data_in, 0, NULL);
        if (pdata.wdt_data.thread == NULL) {
            CloseHandle(child_stdout_r);
            DisconnectNamedPipe(child_stdout_w);
            CloseHandle(child_stdout_w);
            if (wdt_data->watchdog_enabled) {
                UnmapViewOfFile(wdt_data->privdata);
                CloseHandle(hMapFile);
            } else {
                free(wdt_data->privdata);
            }
            wdt_data->privdata = NULL;
            return false;
        }
    } else {
        ResumeThread(pi.hThread);
        // Pass the process handle only
        pdata.wdt_data.sub_Process = pi.hProcess;
        CloseHandle(pi.hThread);
    }
    CopyMemory(wdt_data->privdata, &pdata,
               sizeof(struct popen_wdt_windows_priv));
    return true;
}

void popen_watchdog_stop(popen_watchdog_data_t** data_in) {
    popen_watchdog_data_t* data = NULL;
    struct popen_wdt_windows_priv* pdata = NULL;

    if (!check_data_privdata(data_in)) {
        return;
    }
    data = *data_in;
    pdata = data->privdata;
    if (data->watchdog_enabled) {
        WaitForSingleObject(pdata->wdt_data.thread, INFINITE);
    }
}

popen_watchdog_exit_t popen_watchdog_destroy(popen_watchdog_data_t** data) {
    popen_watchdog_exit_t ret = POPEN_WDT_EXIT_INITIALIZER;
    DWORD exitcode = 0;

    if (!check_data_privdata(data)) {
        return ret;
    }
    struct popen_wdt_windows_priv* pdata = (*data)->privdata;

    POPEN_WDT_DBGLOG("Starting cleanup");
    if (GetExitCodeProcess(pdata->wdt_data.sub_Process, &exitcode)) {
        ret.exitcode = exitcode;
        ret.signal = (*data)->watchdog_activated;
    } else {
        POPEN_WDT_DBGLOG("GetExitCodeProcess failed with error %lu\n",
                         GetLastError());
    }

    POPEN_WDT_DBGLOG("HANDLEs of pdata are being closed");
    CloseHandle(pdata->read_hdl);
    DisconnectNamedPipe(pdata->write_hdl);
    CloseHandle(pdata->write_hdl);
    if ((*data)->watchdog_enabled) {
        CloseHandle(pdata->wdt_data.sub_Process);
        CloseHandle(pdata->wdt_data.sub_Thread);
        CloseHandle(pdata->wdt_data.thread);
        POPEN_WDT_DBGLOG("pdata is being unmapped");
        UnmapViewOfFile(pdata);
        (*data)->privdata = NULL;
    } else {
        POPEN_WDT_DBGLOG("HANDLEs of pdata are being closed");
        CloseHandle(pdata->wdt_data.sub_Process);
        free(pdata);
        (*data)->privdata = NULL;
    }

    POPEN_WDT_DBGLOG("data ptr is being freed");
    free(*data);
    *data = NULL;
    POPEN_WDT_DBGLOG("Cleanup done");
    return ret;
}

bool popen_watchdog_read(popen_watchdog_data_t** data, char* buf, int size) {
    if (!check_data_privdata(data)) {
        return false;
    }

    OVERLAPPED ol = {0};
    popen_watchdog_data_t* data_ = *data;
    struct popen_wdt_windows_priv* pdata = data_->privdata;
    DWORD bytesRead = 0;
    BOOL result = FALSE;
    DWORD waitResult = 0;
    const int one_sec = 1000;
    BOOL readFileResult = FALSE;

    if ((*data)->watchdog_activated) {
        POPEN_WDT_DBGLOG("watchdog_activated: True, return");
        return false;
    }
    ol.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (ol.hEvent == NULL) {
        POPEN_WDT_DBGLOG("CreateEvent failed");
        return false;
    }
    POPEN_WDT_DBGLOG("ReadFile is being called");
    result = ReadFile(pdata->read_hdl, buf, size, &bytesRead, &ol);
    if (!result && GetLastError() != ERROR_IO_PENDING) {
        POPEN_WDT_DBGLOG("ReadFile failed");
        CloseHandle(ol.hEvent);
        return false;
    }
    HANDLE handles[] = {ol.hEvent, pdata->wdt_data.sub_Process};
    waitResult = WaitForMultipleObjects(
        sizeof(handles) / sizeof(HANDLE), handles, FALSE,
        data_->watchdog_enabled ? data_->sleep_secs * one_sec : INFINITE);
    switch (waitResult) {
        case WAIT_OBJECT_0:
            if (GetOverlappedResult(pdata->read_hdl, &ol, &bytesRead, FALSE)) {
                buf[bytesRead] = '\0';  // Null-terminate the string
                readFileResult = TRUE;
            } else {
                POPEN_WDT_DBGLOG("GetOverlappedResult failed.");
            }
            break;
        case WAIT_FAILED:
            POPEN_WDT_DBGLOG("Wait failed: %ld", GetLastError());
            break;
        default:
            POPEN_WDT_DBGLOG("Unexpected result from WaitForSingleObject: %ld",
                             waitResult);
            break;
    }
    POPEN_WDT_DBGLOG("Ret: %s, bytesRead: %lu",
                     readFileResult ? "true" : "false", bytesRead);
    return readFileResult;
}

bool popen_watchdog_activated(popen_watchdog_data_t** data) {
    popen_watchdog_stop(data);

    if (data != NULL && *data != NULL) {
        POPEN_WDT_DBGLOG("IsWatchdogActivated: %d",
                         (*data)->watchdog_activated);
        return (*data)->watchdog_activated;
    }
    return false;
}
