#include <absl/log/log.h>
#include <api/typedefs.h>
#include <fmt/format.h>
#include <grpcpp/create_channel.h>

#include <TryParseStr.hpp>
#include <cstdlib>
#include <cstring>
#include <database/bot/TgBotDatabaseImpl.hpp>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <string_view>
#include <utils/CommandLine.hpp>

#include "ConfigManager.hpp"
#include "Socket_service.grpc.pb.h"

[[noreturn]] static void usage(const char* argv0, const int exitCode) {
    std::cerr << "Usage: " << argv0
              << " <chat(Id/Name)> <medianame> <connect url>" << std::endl;
    exit(exitCode);
}

int app_main(int argc, char** argv) {
    ChatId chatId = 0;
    CommandLine line{argc, argv};
    auto config = std::make_unique<ConfigManager>(line);

    if (argc != 4) {
        usage(argv[0], EXIT_FAILURE);
    }
    auto backend = std::make_unique<TgBotDatabaseImpl>();
    TgBotDatabaseImpl_load(config.get(), backend.get(), &line);
    if (!backend->isLoaded()) {
        LOG(ERROR) << "Failed to load DB from config";
        return EXIT_FAILURE;
    }

    if (!try_parse(argv[1], &chatId)) {
        // Maybe this is a string
        if (const auto id = backend->getChatId(argv[1]); id) {
            chatId = *id;
        } else {
            LOG(ERROR) << "Failed to find chat ID for name '" << argv[1] << "'";
            usage(argv[0], EXIT_FAILURE);
        }
    }
    auto info = backend->queryMediaInfo(argv[2]);
    if (!info.has_value()) {
        LOG(ERROR) << "Failed to find entry for name '" << argv[2] << "'";
        backend->unload();
        return EXIT_FAILURE;
    } else {
        LOG(INFO) << "Found, sending (fileid " << info->mediaId << ") to chat "
                  << chatId;
    }
    tgbot::proto::socket::SendMessageRequest request;
    request.mutable_file_id()->assign(info->mediaId);
    request.set_chat_id(chatId);
    request.set_file_type(static_cast<tgbot::proto::socket::FileType>(
        static_cast<int>(info->mediaType)));

    auto channel =
        grpc::CreateChannel(argv[3], grpc::InsecureChannelCredentials());
    auto stub = tgbot::proto::socket::SocketService::NewStub(channel);
    grpc::ClientContext context;
    tgbot::proto::socket::GenericResponse response;
    auto status = stub->sendMessage(&context, request, &response);
    if (!status.ok()) {
        LOG(ERROR) << "gRPC call failed: " << status.error_message();
        backend->unload();
        return EXIT_FAILURE;
    }
    if (response.code() != tgbot::proto::socket::GenericResponseCode::Success) {
        LOG(ERROR) << "Failed to send media to chat: " << response.message();
        backend->unload();
        return EXIT_FAILURE;
    }
    LOG(INFO) << "Media sent successfully to chat " << chatId;
    backend->unload();
    return EXIT_SUCCESS;
}
