#pragma once

#ifdef WINDOWS_BUILD
#include <winsock2.h>
#undef interface
#endif

#ifdef WINDOWS_BUILD
using socket_handle_t = SOCKET;
#else
using socket_handle_t = int;
#endif
