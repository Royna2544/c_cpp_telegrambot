#include "popen_wdt.h"
#include <stdlib.h>

bool popen_watchdog_init(popen_watchdog_data_t **data) {
    if (data != NULL) {
        *data = calloc(1, sizeof(popen_watchdog_data_t));
        if (*data != NULL) {
            (*data)->sleep_secs = POPEN_WDT_DEFAULT_SLEEP_SECS;
            return true;
        }
    }
    return false;
}
