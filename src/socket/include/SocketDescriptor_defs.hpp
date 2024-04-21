#pragma once

#ifdef __WIN32
#include <windows.h>
#endif

#ifdef __WIN32
using socket_handle_t = SOCKET;
#else
using socket_handle_t = int;
#endif
