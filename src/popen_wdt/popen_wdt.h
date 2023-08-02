#pragma once

#include <stdbool.h>
#include <stdio.h>

#define _STR(x) #x
#define STR(x) _STR(x)

#define SLEEP_SECONDS 10
#define WDT_BITE_STR "-> Intercept: This task was hanging more than " STR(SLEEP_SECONDS) "s"

#ifdef __WIN32
#include <windows.h>
typedef HANDLE POPEN_WDT_HANDLE;
#else
typedef int POPEN_WDT_HANDLE;
#endif

#ifdef __cplusplus
extern "C" {
#endif

FILE* popen_watchdog(const char* command, const POPEN_WDT_HANDLE pipefd);
void unblockForHandle(const POPEN_WDT_HANDLE fd);
bool InitPipeHandle(POPEN_WDT_HANDLE (*fd)[2]);
void writeBoolToHandle(const POPEN_WDT_HANDLE fd, bool value);
bool readBoolFromHandle(const POPEN_WDT_HANDLE fd);
void closeHandle(const POPEN_WDT_HANDLE fd);
extern POPEN_WDT_HANDLE invalid_fd_value;

#ifdef __cplusplus
}
#endif
