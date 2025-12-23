#include <AbslLogCompat.hpp>
#include <absl/strings/ascii.h>

#include <ClientBackend.hpp>
#include <cstdlib>
#include <libos/libsighandler.hpp>

#include "SpdlogInit.hpp"
#include "LogcatData.hpp"

int app_main(int, char**) {
    TgBotSocket::SocketClientWrapper wrapper;
    LogEntry entry{};

    if (!wrapper.connect(TgBotSocket::Context::kTgBotLogPort,
                         {})) {
        SPDLOG_ERROR("Failed to create socket and connect");
        return EXIT_FAILURE;
    }

    SignalHandler::install();

    SPDLOG_INFO("Now waiting to read from the server's logs");

    while (!SignalHandler::isSignaled()) {
        auto data = wrapper->read(sizeof(LogEntry));
        if (!data) {
            return EXIT_FAILURE;
        }
        data->assignTo(entry);
        if (entry.magic != LOGMSG_MAGIC) {
            SPDLOG_ERROR("Invalid magic number");
            return EXIT_FAILURE;
        }
        SPDLOG_INFO("{} {}", static_cast<int>(entry.severity), entry.message.data());
    }
    wrapper->close();
    return EXIT_SUCCESS;
}
