#include <absl/log/log.h>
#include <absl/strings/ascii.h>
#include <libos/libsighandler.hpp>

#include <cstdlib>
#include <impl/backends/ClientBackend.hpp>

#include "AbslLogInit.hpp"
#include "LogcatData.hpp"
#include "SocketBase.hpp"

int main() {
    TgBot_AbslLogInit();

    SocketClientWrapper wrapper(getSocketPathForLogging());
    LogEntry entry{};

    wrapper->options.port = SocketInterfaceBase::kTgBotLogPort;
    auto clientSocket = wrapper->createClientSocket();
    if (!clientSocket) {
        LOG(ERROR) << "Failed to create client socket";
        return EXIT_FAILURE;
    }

    SignalHandler::install();

    LOG(INFO) << "Now waiting to read from the server's logs";

    while (!SignalHandler::isSignaled()) {
        auto data =
            wrapper->readFromSocket(clientSocket.value(), sizeof(LogEntry));
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
    wrapper->closeSocketHandle(clientSocket.value());
    return EXIT_SUCCESS;
}