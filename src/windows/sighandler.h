#pragma once

#include <windows.h>
#include <sysdep/cdefs.h>

_BEGIN_DECLS
BOOL installHandler(void (*cleanup)());
_END_DECLS
