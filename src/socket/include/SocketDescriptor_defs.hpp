#pragma once

#ifdef WINDOWS_BUILD
#include <winsock2.h>
#include <ws2tcpip.h>
#undef interface
#else
#include <sys/socket.h>
#endif

#ifdef WINDOWS_BUILD
using socket_handle_t = SOCKET;
#else
using socket_handle_t = int;
#endif
