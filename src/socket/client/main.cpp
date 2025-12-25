#include <absl/log/log.h>

#include <CommandMap.hpp>
#include <cstdlib>
#include <iostream>

#include "ClientBackend.hpp"
#include "client/CallbackHandler.hpp"
#include "client/ChunkedTransferClient.hpp"
#include "client/CommandParser.hpp"
#include "client/PacketBuilder.hpp"
#include "client/SessionManager.hpp"

using namespace TgBotSocket;
using namespace TgBotSocket::Client;

namespace {

[[noreturn]] void usage(const char* argv, bool success) {
    std::cout << "Usage: " << argv << " [cmd enum value] [args...]" << std::endl
              << std::endl;
    std::cout << "Available cmd enum values:" << std::endl;
    std::cout << CommandHelpers::getHelpText();
    exit(static_cast<int>(!success));
}

bool verifyArgsCount(Command cmd, int argc) {
    int required = CommandHelpers::toCount(cmd);
    if (required != argc) {
        LOG(ERROR) << fmt::format(
            "Invalid argument count {} for cmd {}, {} required.", argc, cmd,
            required);
        return false;
    }
    return true;
}

}  // namespace

int app_main(int argc, char** argv) {
    const char* exe = argv[0];

    // Check if we have enough arguments
    if (argc == 1) {
        usage(exe, true);
    }

    // Parse command
    ++argv;
    --argc;

    auto cmd = CommandParser::parseCommand(*argv);
    if (!cmd) {
        LOG(ERROR) << "Invalid cmd enum value";
        usage(exe, false);
    }

    if (CommandHelpers::isInternalCommand(*cmd)) {
        LOG(ERROR) << "Internal commands not supported";
        return EXIT_FAILURE;
    }

    // Verify argument count
    ++argv;
    --argc;

    if (!verifyArgsCount(*cmd, argc)) {
        usage(exe, false);
    }

    // Connect to server
    SocketClientWrapper backend;
    if (!backend.connect(Context::kTgBotHostPort, Context::hostPath())) {
        LOG(ERROR) << "Failed to connect to server";
        return EXIT_FAILURE;
    }

    DLOG(INFO) << "Connected to server";

    // Open session
    SessionManager sessionMgr(backend);
    auto session_token = sessionMgr.openSession();
    if (!session_token) {
        return EXIT_FAILURE;
    }

    // Build and send command packet
    auto pkt = PacketBuilder::buildPacket(*cmd, argv, *session_token);
    if (!pkt) {
        LOG(ERROR) << "Failed to build packet";
        usage(exe, false);
    }

    backend->write(*pkt);
    LOG(INFO) << "Sent the command: Waiting for callback...";

    // Read and handle response
    auto response = readPacket(backend.chosen_interface());
    if (response) {
        // Check if server wants to use chunked transfer
        if (response->header.cmd == Command::CMD_TRANSFER_FILE_BEGIN) {
            LOG(INFO) << "Server initiated chunked transfer";
            CallbackHandler::handle(response.value(), &backend, &*session_token);
        } else {
            CallbackHandler::handle(response.value());
        }
    }

    // Close session
    sessionMgr.closeSession(*session_token);

    return EXIT_SUCCESS;
}