#pragma once

#include <absl/log/log.h>

#define LOG_ONCE(level) LOG_EVERY_N(level, 1)