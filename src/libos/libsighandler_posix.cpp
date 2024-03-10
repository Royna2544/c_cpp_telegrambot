#include <csignal>

#include "libsighandler.h"

void installSignalHandler(const exit_handler_t et) {
    std::signal(SIGINT, et);
    std::signal(SIGTERM, et);
}
