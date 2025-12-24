#include <absl/log/log.h>
#include <absl/strings/ascii.h>

#include <ClientBackend.hpp>
#include <cstdlib>
#include <iostream>
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

    LOG(INFO) << "Now waiting to read from the server's logs (Press Ctrl-C to exit)";

    while (true) {
        auto data = wrapper->read(sizeof(LogEntry));
        if (!data) {
            // Timeout or error - check if we should exit
            if (SignalHandler::isSignaled()) {
                // Ask user if they want to exit
                std::cout << "\nReceived interrupt signal. Do you want to exit? (y/n): ";
                std::cout.flush();
                
                char response;
                if (std::cin >> response) {
                    if (response == 'y' || response == 'Y') {
                        LOG(INFO) << "Exiting as requested by user";
                        break;
                    } else {
                        LOG(INFO) << "Continuing to wait for logs...";
                        // Reinstall signal handler to reset the flag
                        SignalHandler::uninstall();
                        SignalHandler::install();
                    }
                } else {
                    // Input failed, assume exit
                    break;
                }
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
