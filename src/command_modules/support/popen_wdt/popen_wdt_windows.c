#include <Windows.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#define POPEN_WDT_BUFSIZ                  (1 << 10)
#define POPEN_WDT_KILL_GRACE_MILLIS       2000
#define POPEN_WDT_TERMINATION_POLL_MILLIS 20

static volatile LONG popen_wdt_pipe_sequence = 0;

struct popen_wdt_windows_priv {
    struct {
        HANDLE sub_Process;
        HANDLE sub_Thread;
        HANDLE thread;
    } wdt_data;
    HANDLE job;
    HANDLE read_hdl;
    HANDLE write_hdl;
    HANDLE termination_done;
    DWORD process_group_id;
    volatile LONG termination_started;
    volatile LONG activated;
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
        free(*data);
        *data = NULL;
        return false;
    }
    return true;
}

static bool job_has_active_processes(HANDLE job) {
    if (job == NULL) {
        return false;
    }
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION info = {0};
    if (!QueryInformationJobObject(job, JobObjectBasicAccountingInformation,
                                   &info, sizeof(info), NULL)) {
        POPEN_WDT_DBGLOG("QueryInformationJobObject failed with error %lu",
                         GetLastError());
        // Treat an unknown state as active so cancellation fails closed.
        return true;
    }
    return info.ActiveProcesses != 0;
}

static bool terminate_job_gracefully(popen_watchdog_data_t* data) {
    struct popen_wdt_windows_priv* pdata = data->privdata;
    if (InterlockedCompareExchange(&pdata->termination_started, 1, 0) != 0) {
        (void)WaitForSingleObject(pdata->termination_done, INFINITE);
        return true;
    }

    InterlockedExchange(&pdata->activated, 1);

    // CREATE_NEW_PROCESS_GROUP makes the primary PID the group ID. This is a
    // best-effort graceful notification: services without an attached console
    // cannot generate it, but the Job Object fallback below remains reliable.
    if (!GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, pdata->process_group_id)) {
        POPEN_WDT_DBGLOG("GenerateConsoleCtrlEvent failed with error %lu",
                         GetLastError());
    }

    const ULONGLONG deadline = GetTickCount64() + POPEN_WDT_KILL_GRACE_MILLIS;
    while (job_has_active_processes(pdata->job) &&
           GetTickCount64() < deadline) {
        Sleep(POPEN_WDT_TERMINATION_POLL_MILLIS);
    }

    bool success = true;
    if (job_has_active_processes(pdata->job)) {
        if (!TerminateJobObject(pdata->job, POPEN_WDT_SIGKILL)) {
            POPEN_WDT_DBGLOG("TerminateJobObject failed with error %lu",
                             GetLastError());
            // KILL_ON_JOB_CLOSE is a second, independent containment path.
            (void)CloseHandle(pdata->job);
            pdata->job = NULL;
            success = false;
        }
    }
    if (WaitForSingleObject(pdata->wdt_data.sub_Process,
                            POPEN_WDT_KILL_GRACE_MILLIS) == WAIT_TIMEOUT) {
        (void)TerminateProcess(pdata->wdt_data.sub_Process, POPEN_WDT_SIGKILL);
        (void)WaitForSingleObject(pdata->wdt_data.sub_Process,
                                  POPEN_WDT_KILL_GRACE_MILLIS);
        success = false;
    }
    (void)SetEvent(pdata->termination_done);
    return success;
}

static void wait_for_primary_process(struct popen_wdt_windows_priv* pdata) {
    for (;;) {
        const DWORD wait = WaitForSingleObject(
            pdata->wdt_data.sub_Process, POPEN_WDT_TERMINATION_POLL_MILLIS);
        if (wait != WAIT_TIMEOUT) {
            return;
        }
        if (InterlockedCompareExchange(&pdata->termination_started, 0, 0) !=
            0) {
            (void)WaitForSingleObject(pdata->wdt_data.sub_Process,
                                      POPEN_WDT_KILL_GRACE_MILLIS);
            return;
        }
    }
}

static void close_parent_pipe_writer(struct popen_wdt_windows_priv* pdata) {
    HANDLE writer = (HANDLE)InterlockedExchangePointer(
        (PVOID volatile*)&pdata->write_hdl, NULL);
    if (writer != NULL && writer != INVALID_HANDLE_VALUE) {
        (void)DisconnectNamedPipe(writer);
        (void)CloseHandle(writer);
    }
}

static DWORD WINAPI watchdog(LPVOID arg) {
    popen_watchdog_data_t* data = (popen_watchdog_data_t*)arg;
    struct popen_wdt_windows_priv* pdata = data->privdata;
    const ULONGLONG end_time = GetTickCount64() + data->sleep_secs * 1000ULL;

    if (ResumeThread(pdata->wdt_data.sub_Thread) == (DWORD)-1) {
        POPEN_WDT_DBGLOG("ResumeThread failed with error %lu", GetLastError());
        (void)terminate_job_gracefully(data);
        close_parent_pipe_writer(pdata);
        return 0;
    }

    while (job_has_active_processes(pdata->job)) {
        if (InterlockedCompareExchange(&pdata->termination_started, 0, 0) !=
            0) {
            (void)WaitForSingleObject(pdata->termination_done,
                                      POPEN_WDT_KILL_GRACE_MILLIS * 2);
            break;
        }
        if (GetTickCount64() >= end_time) {
            POPEN_WDT_DBGLOG("Watchdog deadline reached");
            (void)terminate_job_gracefully(data);
            break;
        }
        Sleep(100);
    }

    close_parent_pipe_writer(pdata);
    return 0;
}

static void free_privdata(popen_watchdog_data_t* data) {
    struct popen_wdt_windows_priv* pdata = data->privdata;
    if (pdata == NULL) {
        return;
    }
    if (pdata->read_hdl != NULL && pdata->read_hdl != INVALID_HANDLE_VALUE) {
        (void)CloseHandle(pdata->read_hdl);
    }
    close_parent_pipe_writer(pdata);
    if (pdata->wdt_data.thread != NULL) {
        (void)CloseHandle(pdata->wdt_data.thread);
    }
    if (pdata->wdt_data.sub_Process != NULL) {
        (void)CloseHandle(pdata->wdt_data.sub_Process);
    }
    if (pdata->wdt_data.sub_Thread != NULL) {
        (void)CloseHandle(pdata->wdt_data.sub_Thread);
    }
    if (pdata->job != NULL) {
        // JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE guarantees that an escaped
        // descendant cannot outlive watchdog cleanup.
        (void)CloseHandle(pdata->job);
    }
    if (pdata->termination_done != NULL) {
        (void)CloseHandle(pdata->termination_done);
    }
    free(pdata);
    data->privdata = NULL;
}

static char* duplicate_command_line(const char* command) {
    const size_t size = strlen(command) + 1;
    char* copy = malloc(size);
    if (copy != NULL) {
        memcpy(copy, command, size);
    }
    return copy;
}

static char* make_shell_command_line(const char* command) {
    static const char prefix[] = POPEN_WDT_DEFAULT_SHELL " -c \"";
    const size_t size = sizeof(prefix) - 1 + strlen(command) + 2;
    char* line = malloc(size);
    if (line != NULL) {
        (void)snprintf(line, size, "%s%s\"", prefix, command);
    }
    return line;
}

bool popen_watchdog_start(popen_watchdog_data_t** data_in) {
    if (data_in == NULL || *data_in == NULL || (*data_in)->command == NULL) {
        return false;
    }

    popen_watchdog_data_t* data = *data_in;
    struct popen_wdt_windows_priv* pdata = calloc(1, sizeof(*pdata));
    if (pdata == NULL) {
        return false;
    }
    data->privdata = pdata;

    SECURITY_ATTRIBUTES inherit = {
        .nLength = sizeof(SECURITY_ATTRIBUTES),
        .lpSecurityDescriptor = NULL,
        .bInheritHandle = TRUE,
    };
    STARTUPINFOEXA startup = {0};
    PROCESS_INFORMATION process = {0};
    char pipe_name[128] = {0};
    const LONG pipe_sequence = InterlockedIncrement(&popen_wdt_pipe_sequence);
    (void)snprintf(pipe_name, sizeof(pipe_name),
                   "\\\\.\\pipe\\popen_wdt_%lu_%ld", GetCurrentProcessId(),
                   pipe_sequence);
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;

    pdata->termination_done = CreateEventA(NULL, TRUE, FALSE, NULL);
    pdata->job = CreateJobObjectA(NULL, NULL);
    if (pdata->termination_done == NULL || pdata->job == NULL) {
        free_privdata(data);
        return false;
    }
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {0};
    limits.BasicLimitInformation.LimitFlags =
        JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(pdata->job, JobObjectExtendedLimitInformation,
                                 &limits, sizeof(limits))) {
        POPEN_WDT_DBGLOG("SetInformationJobObject failed with error %lu",
                         GetLastError());
        free_privdata(data);
        return false;
    }

    pdata->write_hdl = startup.StartupInfo.hStdError =
        startup.StartupInfo.hStdOutput = CreateNamedPipeA(
            pipe_name, PIPE_ACCESS_OUTBOUND | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_BYTE | PIPE_WAIT, PIPE_UNLIMITED_INSTANCES,
            POPEN_WDT_BUFSIZ, POPEN_WDT_BUFSIZ, 0, &inherit);
    if (pdata->write_hdl == INVALID_HANDLE_VALUE) {
        pdata->write_hdl = NULL;
        free_privdata(data);
        return false;
    }
    pdata->read_hdl = CreateFileA(pipe_name, GENERIC_READ, 0, NULL,
                                  OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    if (pdata->read_hdl == INVALID_HANDLE_VALUE) {
        pdata->read_hdl = NULL;
        free_privdata(data);
        return false;
    }

    // bInheritHandles alone is process-global: concurrent starts could make
    // each child retain the other command's pipe writer. Restrict inheritance
    // to this command's stdout/stderr handle so independent reads reach EOF.
    SIZE_T attribute_bytes = 0;
    (void)InitializeProcThreadAttributeList(NULL, 1, 0, &attribute_bytes);
    startup.lpAttributeList = malloc(attribute_bytes);
    if (startup.lpAttributeList == NULL ||
        !InitializeProcThreadAttributeList(startup.lpAttributeList, 1, 0,
                                           &attribute_bytes)) {
        free(startup.lpAttributeList);
        startup.lpAttributeList = NULL;
        free_privdata(data);
        return false;
    }
    HANDLE inherited_handles[] = {pdata->write_hdl};
    if (!UpdateProcThreadAttribute(
            startup.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
            inherited_handles, sizeof(inherited_handles), NULL, NULL)) {
        DeleteProcThreadAttributeList(startup.lpAttributeList);
        free(startup.lpAttributeList);
        startup.lpAttributeList = NULL;
        free_privdata(data);
        return false;
    }

    char* command_line = duplicate_command_line(data->command);
    BOOL created = command_line != NULL &&
                   CreateProcessA(NULL, command_line, NULL, NULL, TRUE,
                                  CREATE_NEW_PROCESS_GROUP | CREATE_SUSPENDED |
                                      EXTENDED_STARTUPINFO_PRESENT,
                                  NULL, NULL, &startup.StartupInfo, &process);
    free(command_line);
    if (!created) {
        command_line = make_shell_command_line(data->command);
        created = command_line != NULL &&
                  CreateProcessA(NULL, command_line, NULL, NULL, TRUE,
                                 CREATE_NEW_PROCESS_GROUP | CREATE_SUSPENDED |
                                     EXTENDED_STARTUPINFO_PRESENT,
                                 NULL, NULL, &startup.StartupInfo, &process);
        free(command_line);
    }
    DeleteProcThreadAttributeList(startup.lpAttributeList);
    free(startup.lpAttributeList);
    startup.lpAttributeList = NULL;
    if (!created) {
        POPEN_WDT_DBGLOG("CreateProcess failed with error %lu", GetLastError());
        free_privdata(data);
        return false;
    }

    pdata->wdt_data.sub_Process = process.hProcess;
    pdata->wdt_data.sub_Thread = process.hThread;
    pdata->process_group_id = process.dwProcessId;
    if (!AssignProcessToJobObject(pdata->job, process.hProcess)) {
        POPEN_WDT_DBGLOG("AssignProcessToJobObject failed with error %lu",
                         GetLastError());
        (void)TerminateProcess(process.hProcess, POPEN_WDT_SIGKILL);
        (void)WaitForSingleObject(process.hProcess, INFINITE);
        free_privdata(data);
        return false;
    }

    if (data->watchdog_enabled) {
        pdata->wdt_data.thread = CreateThread(NULL, 0, watchdog, data, 0, NULL);
        if (pdata->wdt_data.thread == NULL) {
            (void)TerminateJobObject(pdata->job, POPEN_WDT_SIGKILL);
            (void)WaitForSingleObject(process.hProcess, INFINITE);
            free_privdata(data);
            return false;
        }
    } else if (ResumeThread(process.hThread) == (DWORD)-1) {
        POPEN_WDT_DBGLOG("ResumeThread failed with error %lu", GetLastError());
        (void)TerminateJobObject(pdata->job, POPEN_WDT_SIGKILL);
        (void)WaitForSingleObject(process.hProcess, INFINITE);
        free_privdata(data);
        return false;
    }

    POPEN_WDT_DBGLOG("Process PID: %lu, TID: %lu", process.dwProcessId,
                     process.dwThreadId);
    return true;
}

popen_watchdog_exit_t popen_watchdog_destroy(popen_watchdog_data_t** data_in) {
    popen_watchdog_exit_t result = POPEN_WDT_EXIT_INITIALIZER;
    if (!check_data_privdata(data_in)) {
        return result;
    }

    popen_watchdog_data_t* data = *data_in;
    struct popen_wdt_windows_priv* pdata = data->privdata;
    if (pdata->wdt_data.thread != NULL) {
        (void)WaitForSingleObject(pdata->wdt_data.thread, INFINITE);
    } else {
        wait_for_primary_process(pdata);
    }

    DWORD exit_code = 0;
    if (GetExitCodeProcess(pdata->wdt_data.sub_Process, &exit_code)) {
        result.signal =
            InterlockedCompareExchange(&pdata->activated, 0, 0) != 0;
        result.exitcode = exit_code == STILL_ACTIVE && result.signal
                              ? POPEN_WDT_SIGKILL
                              : exit_code;
    }
    data->watchdog_activated = result.signal;
    free_privdata(data);
    free(data);
    *data_in = NULL;
    return result;
}

popen_watchdog_ssize_t popen_watchdog_read(popen_watchdog_data_t** data_in,
                                           char* buf,
                                           popen_watchdog_ssize_t size) {
    if (!check_data_privdata(data_in) || buf == NULL || size <= 0) {
        return -1;
    }
    popen_watchdog_data_t* data = *data_in;
    struct popen_wdt_windows_priv* pdata = data->privdata;
    if (InterlockedCompareExchange(&pdata->activated, 0, 0) != 0) {
        return -1;
    }

    OVERLAPPED operation = {0};
    operation.hEvent = CreateEventA(NULL, FALSE, FALSE, NULL);
    if (operation.hEvent == NULL) {
        return -1;
    }

    const DWORD read_size =
        size > (popen_watchdog_ssize_t)MAXDWORD ? MAXDWORD : (DWORD)size;
    DWORD bytes_read = 0;
    const BOOL read_started =
        ReadFile(pdata->read_hdl, buf, read_size, &bytes_read, &operation);
    if (!read_started && GetLastError() != ERROR_IO_PENDING) {
        (void)CloseHandle(operation.hEvent);
        return -1;
    }

    HANDLE handles[] = {operation.hEvent, pdata->wdt_data.sub_Process,
                        pdata->termination_done};
    const DWORD timeout =
        data->watchdog_enabled ? (DWORD)data->sleep_secs * 1000U : INFINITE;
    const DWORD wait = WaitForMultipleObjects(
        sizeof(handles) / sizeof(handles[0]), handles, FALSE, timeout);
    if (wait == WAIT_OBJECT_0) {
        if (!GetOverlappedResult(pdata->read_hdl, &operation, &bytes_read,
                                 FALSE)) {
            bytes_read = 0;
        }
    } else {
        DWORD ignored = 0;
        (void)CancelIoEx(pdata->read_hdl, &operation);
        (void)GetOverlappedResult(pdata->read_hdl, &operation, &ignored, TRUE);
        bytes_read = 0;
    }
    (void)CloseHandle(operation.hEvent);
    return bytes_read == 0 ? -1 : (popen_watchdog_ssize_t)bytes_read;
}

bool popen_watchdog_activated(popen_watchdog_data_t** data_in) {
    if (!check_data_privdata(data_in)) {
        return false;
    }
    popen_watchdog_data_t* data = *data_in;
    struct popen_wdt_windows_priv* pdata = data->privdata;
    if (pdata->wdt_data.thread != NULL) {
        (void)WaitForSingleObject(pdata->wdt_data.thread, INFINITE);
    } else {
        wait_for_primary_process(pdata);
    }
    const bool activated =
        InterlockedCompareExchange(&pdata->activated, 0, 0) != 0;
    data->watchdog_activated = activated;
    return activated;
}

bool popen_watchdog_cancel(popen_watchdog_data_t** data_in) {
    if (!check_data_privdata(data_in)) {
        return false;
    }
    popen_watchdog_data_t* data = *data_in;
    struct popen_wdt_windows_priv* pdata = data->privdata;
    if (!job_has_active_processes(pdata->job)) {
        return false;
    }
    return terminate_job_gracefully(data);
}
