#include <api/typedefs.h>
#include <absl/log/log.h>
#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <TryParseStr.hpp>
#include <ClientBackend.hpp>
#include <bot/PacketParser.hpp>
#include <cstdlib>
#include <cstring>
#include <database/bot/TgBotDatabaseImpl.hpp>
#include <iostream>
#include <memory>

#include "ConfigManager.hpp"

[[noreturn]] static void usage(const char* argv0, const int exitCode) {
    std::cerr << "Usage: " << argv0 << " <chat(Id/Name)> <medianame>"
              << std::endl;
    exit(exitCode);
}

int app_main(int argc, char** argv) {
    ChatId chatId = 0;
    TgBotSocket::data::SendFileToChatId data = {};
    const auto _usage = [capture0 = argv[0]](auto&& PH1) {
        usage(capture0, std::forward<decltype(PH1)>(PH1));
    };
    CommandLine line{argc, argv};
    auto config = std::make_unique<ConfigManager>(line);

    if (argc != 3) {
        _usage(EXIT_SUCCESS);
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
            _usage(EXIT_FAILURE);
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
    TgBotSocket::copyTo(data.filePath, info->mediaId);
    data.chat = chatId;
    data.fileType = TgBotSocket::data::FileType::TYPE_DOCUMENT;

    TgBotSocket::SocketClientWrapper wrapper;
    if (wrapper.connect(TgBotSocket::Context::kTgBotHostPort,
                        TgBotSocket::Context::hostPath())) {
        using namespace TgBotSocket;
        DLOG(INFO) << "Connected to server";
        Packet openSession = createPacket(Command::CMD_OPEN_SESSION, nullptr, 0,
                                          PayloadType::Binary, {});
        wrapper->write(openSession);
        DLOG(INFO) << "Wrote open session packet";
        auto openSessionAck =
            TgBotSocket::readPacket(wrapper.chosen_interface());
        if (!openSessionAck ||
            openSessionAck->header.cmd != Command::CMD_OPEN_SESSION_ACK) {
            LOG(ERROR) << "Failed to open session";
            return EXIT_FAILURE;
        }
        auto _root = parseAndCheck(openSessionAck->data.get(),
                                   openSessionAck->data.size(),
                                   {"session_token", "expiration_time"});
        if (!_root) {
            LOG(ERROR) << "Invalid open session ack json";
            return EXIT_FAILURE;
        }
        auto root = *_root;
        LOG(INFO) << "Opened session. Token: " << root["session_token"]
                  << " expiration_time: " << root["expiration_time"];

        std::string session_token_str = root["session_token"].get<std::string>();
        Packet::Header::session_token_type session_token{};
        copyTo(session_token, session_token_str);
        auto pkt =
            createPacket(TgBotSocket::Command::CMD_SEND_FILE_TO_CHAT_ID, &data,
                         sizeof(data), PayloadType::Binary, session_token);
        if (!wrapper->write(pkt)) {
            LOG(ERROR) << "Failed to write send file to chat id packet";
            backend->unload();
            return EXIT_FAILURE;
        }
        auto result = TgBotSocket::readPacket(wrapper.chosen_interface());
        if (!result || result->header.cmd != Command::CMD_GENERIC_ACK) {
            LOG(ERROR) << "Failed to send file to chat id";
            backend->unload();
            return EXIT_FAILURE;
        }
        TgBotSocket::callback::GenericAck genericAck;
        result->data.assignTo(genericAck);
        if (genericAck.result != TgBotSocket::callback::AckType::SUCCESS) {
            LOG(ERROR) << "Failed to send file to chat id: "
                       << genericAck.error_msg.data();
            backend->unload();
            return EXIT_FAILURE;
        }
        DLOG(INFO) << "File sent successfully";
        backend->unload();
    }
    return EXIT_SUCCESS;
}
