#include <csignal>
#include <cstdlib>

#include "handler.h"

void _installExitHandler(void) {
    std::signal(SIGINT, exitHandlerInt);
    std::signal(SIGTERM, exitHandlerInt);
    std::atexit(exitHandlerVoid);
}
