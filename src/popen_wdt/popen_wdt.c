#ifdef __WIN32
#include "popen_wdt_windows.c"
#else
#include "popen_wdt_posix.c"
#endif