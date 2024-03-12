#pragma once

#include <stdbool.h>
#include <stdio.h>

#define _STR(x) #x
#define STR(x) _STR(x)

#define SLEEP_SECONDS 10
#define WDT_BITE_STR \
    "-> Intercept: This task was hanging more than " STR(SLEEP_SECONDS) "s"

#ifndef __ANDROID__
#define BASH_EXE_PATH "/bin/bash"
#else
#define BASH_EXE_PATH "/data/data/com.termux/files/usr/bin/bash"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * setlocale_enus_once - Function called once to set the terminal output to
 * locale en-US
 */
void setlocale_enus_once(void);

/**
 * popen_watchdog - popen(3) but with watchdog support
 *
 * Same as popen, it opens subprocess with [command], but
 * the process is killed, and FILE * returns EOF when the exec time
 * expires [SLEEP_SECONDS]. Also closes stdin and pipes stdout, stderr
 * to FILE *
 *
 * @param command Null-terminated string of command
 * @param ret Whether watchdog killed the process or not,
 * passing nullptr will disable watchdog functionaility.
 * @return valid FILE * if succeeded, nullptr otherwise
 */
FILE* popen_watchdog(const char* command, bool* ret);

#ifdef __cplusplus
}
#endif