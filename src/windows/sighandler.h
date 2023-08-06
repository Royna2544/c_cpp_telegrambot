#pragma once

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

BOOL installHandler(void (*cleanup)());

#ifdef __cplusplus
}
#endif