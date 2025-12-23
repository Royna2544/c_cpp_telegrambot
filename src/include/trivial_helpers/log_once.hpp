#pragma once

#include <AbslLogCompat.hpp>

#define LOG_ONCE(level) LOG_FIRST_N(level, 1)