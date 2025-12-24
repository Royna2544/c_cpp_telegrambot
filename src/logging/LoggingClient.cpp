#include <absl/log/log.h>
#include <absl/strings/ascii.h>

#include <ClientBackend.hpp>
#include <cstdlib>
#include <libos/libsighandler.hpp>

#include "AbslLogInit.hpp"
#include "LogcatData.hpp"

int app_main(int, char**) {
    TgBotSocket::SocketClientWrapper wrapper;
    LogEntry entry{};

    if (!wrapper.connect(TgBotSocket::Context::kTgBotLogPort,
                         {})) {
        LOG(ERROR) << "Failed to create socket and connect";
        return EXIT_FAILURE;
    }

    // Set IO timeout to allow checking for signals periodically
    wrapper->options.io_timeout = std::chrono::seconds(1);

    SignalHandler::install();

    LOG(INFO) << "Now waiting to read from the server's logs";

    while (!SignalHandler::isSignaled()) {
        auto data = wrapper->read(sizeof(LogEntry));
        if (!data) {
            // Timeout or error - check if we should exit
            if (SignalHandler::isSignaled()) {
                break;
            }
            continue;
        }
        data->assignTo(entry);
        if (entry.magic != LOGMSG_MAGIC) {
            LOG(ERROR) << "Invalid magic number";
            return EXIT_FAILURE;
        }
        LOG(INFO) << entry.severity << " " << entry.message.data();
    }
    wrapper->close();
    return EXIT_SUCCESS;
}
