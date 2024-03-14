#pragma once

#include "../Logging.h"

#include <string.h>
#include <errno.h>

#define PLOG_E(fmt) \
    LOG(LogLevel::ERROR, fmt ": %s", strerror(errno))
