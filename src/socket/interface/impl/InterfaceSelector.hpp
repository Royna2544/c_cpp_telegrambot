#pragma once

#ifdef __WIN32
#include "SocketWindows.hpp"

using SocketInternalInterface = SocketInterfaceWindowsLocal;
using SocketExternalInterface = SocketInterfaceWindowsIPv4;
#else // __WIN32
#include "SocketPosix.hpp"

using SocketInternalInterface = SocketInterfaceUnixLocal;
using SocketExternalInterface = SocketInterfaceUnixIPv4;
#endif // __WIN32