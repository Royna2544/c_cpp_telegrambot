#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <ws2ipdef.h>
#include <ws2tcpip.h>
#include <afunix.h>
#include <windows.h>
#else
#include <sys/socket.h> // socklen_t
#endif

#ifdef _WIN32
using socket_handle_t = SOCKET;
#else
using socket_handle_t = int;
#endif
