#pragma once

#ifdef WINDOWS_BUILD
#include <winsock2.h>
#include <windows.h>
#undef interface
#else
#endif

#ifdef WINDOWS_BUILD
using socket_handle_t = SOCKET;
#else
using socket_handle_t = int;
#endif
