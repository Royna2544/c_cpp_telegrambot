#pragma once

#include <stdio.h>

#define SLEEP_SECONDS 10

#ifdef __cplusplus
extern "C" {
#endif

FILE* popen_watchdog(const char* command);

#ifdef __cplusplus
}
#endif
