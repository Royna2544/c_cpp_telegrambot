#pragma once

#include <Logging.h>  // LOG_W

#include "config.h"
#define IS_DEFINED IS_BUILTIN

#define unlikely(x) __builtin_expect(!!(x), 0)

#define WARN_ONCE(condition, format...) ({        \
    static bool __warned;                         \
    int __ret_warn_once = !!(condition);          \
                                                  \
    if (unlikely(__ret_warn_once && !__warned)) { \
        __warned = true;                          \
        LOG_W(format);                            \
    }                                             \
    unlikely(__ret_warn_once);                    \
})
