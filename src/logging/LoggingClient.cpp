#include <absl/base/log_severity.h>
#include <absl/log/log.h>
#include <absl/log/log_entry.h>
#include <absl/log/log_sink.h>
#include <absl/log/log_sink_registry.h>

#include <boost/algorithm/string/trim.hpp>
#include <cstdlib>
#include <impl/bot/ClientBackend.hpp>

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

    LOG(INFO) << "Now waiting to read from the server's logs";

    while (true) {
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
        std::string message = entry.message.data();
        boost::trim(message);
        LOG(INFO) << entry.severity << " " << message;
    }
    return EXIT_SUCCESS;
}