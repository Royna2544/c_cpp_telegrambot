#include <csignal>

#include "libsighandler.h"

void installSignalHandler() {
    std::signal(SIGINT, defaultSignalHandler);
    std::signal(SIGTERM, defaultSignalHandler);
}
