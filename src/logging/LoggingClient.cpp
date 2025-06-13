#include <absl/log/log.h>
#include <absl/strings/ascii.h>

#include <ClientBackend.hpp>
#include <cstdlib>
#include <libos/libsighandler.hpp>

#include "AbslLogInit.hpp"
#include "LogcatData.hpp"

int main() {
    TgBot_AbslLogInit();

    SocketClientWrapper wrapper;
    LogEntry entry{};

    if (!wrapper.connect(TgBotSocket::Context::kTgBotLogPort,
                         {})) {
        LOG(ERROR) << "Failed to create socket and connect";
        return EXIT_FAILURE;
    }

    SignalHandler::install();

    LOG(INFO) << "Now waiting to read from the server's logs";

    while (!SignalHandler::isSignaled()) {
        auto data = wrapper->read(sizeof(LogEntry));
        if (!data) {
            return EXIT_FAILURE;
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
