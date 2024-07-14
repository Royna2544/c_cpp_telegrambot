#include "popen_wdt.h"
#include <stdlib.h>
#include <string.h>

bool popen_watchdog_init(popen_watchdog_data_t **data) {
    if (data != NULL) {
        *data = malloc(sizeof(popen_watchdog_data_t));
        if (*data != NULL) {
            memset(*data, 0, sizeof(popen_watchdog_data_t));
            return true;
        }
    }
    return false;
}
