#pragma once

#include <stdbool.h>
#include <stdio.h>
#include <cdefs.h>

#define _STR(x) #x
#define STR(x) _STR(x)

#define SLEEP_SECONDS 10
#define WDT_BITE_STR "-> Intercept: This task was hanging more than " STR(SLEEP_SECONDS) "s"

_BEGIN_DECLS
void setlocale_enus_once(void);
FILE* popen_watchdog(const char* command, bool* ret);
_END_DECLS
