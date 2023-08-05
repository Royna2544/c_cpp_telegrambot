#pragma once

#include <stdbool.h>
#include <stdio.h>

#define _STR(x) #x
#define STR(x) _STR(x)

#define SLEEP_SECONDS 10
#define WDT_BITE_STR "-> Intercept: This task was hanging more than " STR(SLEEP_SECONDS) "s"

#ifdef __cplusplus
extern "C" {
#endif

FILE* popen_watchdog(const char* command, bool* ret);

#ifdef __cplusplus
}
#endif
