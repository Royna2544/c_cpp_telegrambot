#include "popen_wdt.h"
#include <stdlib.h>
#include <string.h>

bool popen_watchdog_init(popen_watchdog_data_t **data) {
    if (data != NULL) {
        *data = calloc(1, sizeof(popen_watchdog_data_t));
        if (*data != NULL) {
            (*data)->sleep_secs = 10;
            return true;
        }
    }
    return false;
}
