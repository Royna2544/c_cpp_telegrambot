#include <Log_service.grpc.pb.h>
#include <absl/log/log.h>
#include <absl/strings/ascii.h>
#include <grpcpp/create_channel.h>

#include <cstdlib>
#include <iostream>
#include <libos/libsighandler.hpp>

#include "LogcatData.hpp"

using tgbot::proto::logging::LogData;
using tgbot::proto::logging::LoggingService;

std::ostream& operator<<(std::ostream& os,
                         const tgbot::proto::logging::LogSeverity& severity) {
    switch (severity) {
        case tgbot::proto::logging::LogSeverity::Info:
            os << "INFO";
            break;
        case tgbot::proto::logging::LogSeverity::Warning:
            os << "WARNING";
            break;
        case tgbot::proto::logging::LogSeverity::Error:
            os << "ERROR";
            break;
        case tgbot::proto::logging::LogSeverity::Fatal:
            os << "FATAL";
            break;
        default:
            os << "UNKNOWN";
            break;
    }
    return os;
}

int app_main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <server_address> <auth_token>\n";
        return EXIT_FAILURE;
    }

    SignalHandler::install();

    auto channel =
        grpc::CreateChannel(argv[1], grpc::InsecureChannelCredentials());
    auto stub = LoggingService::NewStub(channel);

    grpc::ClientContext context;
    google::protobuf::Empty request;
    auto reader = stub->getLogs(&context, request);

    LOG(INFO)
        << "Now waiting to read from the server's logs (Press Ctrl-C to exit)";

    while (true) {
        LogData entry;
        if (!reader->Read(&entry)) {
            // Stream has ended
            LOG(INFO) << "Log stream ended by server";
            break;
        }
        // Timeout or error - check if we should exit
        if (SignalHandler::isSignaled()) {
            // Ask user if they want to exit
            std::cout << "\nReceived interrupt signal. Do you want to "
                         "exit? (y/n): ";
            std::cout.flush();

            char response = 0;
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

        LOG(INFO) << std::chrono::system_clock::from_time_t(entry.timestamp())
                  << " " << entry.severity() << " " << entry.message();
    }
    grpc::Status status = reader->Finish();
    if (!status.ok()) {
        LOG(ERROR) << "getLogs rpc failed: " << status.error_message();
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
