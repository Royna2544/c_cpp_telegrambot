#include <Types.h>
#include <absl/log/log.h>

#include <AbslLogInit.hpp>
#include <TryParseStr.hpp>
#include <cstdlib>
#include <cstring>
#include <database/bot/TgBotDatabaseImpl.hpp>
#include <backends/ClientBackend.hpp>
#include <iostream>
#include <memory>
#include "ConfigManager.hpp"

[[noreturn]] static void usage(const char* argv0, const int exitCode) {
    std::cerr << "Usage: " << argv0 << " <chat(Id/Name)> <medianame>"
              << std::endl;
    exit(exitCode);
}

int main(int argc, char** argv) {
    ChatId chatId = 0;
    TgBotSocket::data::SendFileToChatId data = {};
    const auto _usage = [capture0 = argv[0]](auto&& PH1) {
        usage(capture0, std::forward<decltype(PH1)>(PH1));
    };
    TgBot_AbslLogInit();
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
    copyTo(data.filePath, info->mediaId.c_str());
    data.chat = chatId;
    data.fileType = TgBotSocket::data::FileType::TYPE_DOCUMENT;

    struct TgBotSocket::Packet pkt(
        TgBotSocket::Command::CMD_SEND_FILE_TO_CHAT_ID, data);
    SocketClientWrapper wrapper;
    wrapper.connect(TgBotSocket::Context::kTgBotHostPort, TgBotSocket::Context::hostPath());
    wrapper->write(pkt);
    backend->unload();
}
