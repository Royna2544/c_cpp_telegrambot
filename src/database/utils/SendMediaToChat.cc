#include <Types.h>
#include <absl/log/log.h>

#include <AbslLogInit.hpp>
#include <CommandLine.hpp>
#include <cstdlib>
#include <cstring>
#include <impl/bot/ClientBackend.hpp>
#include <iostream>

#include "TryParseStr.hpp"
#include "database/bot/TgBotDatabaseImpl.hpp"

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
    CommandLine::initInstance(argc, argv);

    if (argc != 3) {
        _usage(EXIT_SUCCESS);
    }
    auto backend = TgBotDatabaseImpl::getInstance();
    backend->initWrapper();
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
        backend->unloadDatabase();
        return EXIT_FAILURE;
    } else {
        LOG(INFO) << "Found, sending (fileid " << info->mediaId << ") to chat "
                  << chatId;
    }
    strncpy(data.filePath.data(), info->mediaId.c_str(), data.filePath.size());
    data.chat = chatId;
    data.fileType = TgBotSocket::data::FileType::TYPE_DOCUMENT;

    struct TgBotSocket::Packet pkt(
        TgBotSocket::Command::CMD_SEND_FILE_TO_CHAT_ID, data);
    SocketClientWrapper wrapper(
        SocketInterfaceBase::LocalHelper::getSocketPath());
    wrapper->writeAsClientToSocket(pkt.toSocketData());
    backend->unloadDatabase();
}
