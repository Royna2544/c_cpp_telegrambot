#include <Types.h>
#include <socket/TgBotSocket.h>

#include <AbslLogInit.hpp>
#include <DatabaseBot.hpp>
#include <cstdlib>
#include <cstring>
#include <impl/bot/ClientBackend.hpp>
#include <iostream>
#include <string>
#include "TgBotCommandExport.hpp"


[[noreturn]] static void usage(const char* argv0, const int exitCode) {
    std::cerr << "Usage: " << argv0 << " <chatId> <name stored in DB>"
              << std::endl;
    exit(exitCode);
}

int main(int argc, char* const* argv) {
    ChatId chatId = 0;
    TgBotSocket::data::SendFileToChatId data = {};
    const auto _usage = [capture0 = argv[0]](auto&& PH1) {
        usage(capture0, std::forward<decltype(PH1)>(PH1));
    };
    auto backend = DefaultDatabase();

    TgBot_AbslLogInit();
    if (argc != 3) {
        _usage(EXIT_SUCCESS);
    }
    backend.loadDatabaseFromFile(DefaultBotDatabase::getDatabaseDefaultPath());
    try {
        chatId = std::stoll(argv[1]);
    } catch (...) {
        LOG(ERROR) << "Failed to convert '" << argv[1] << "' to ChatId";
        _usage(EXIT_FAILURE);
    }
    auto info = backend.queryMediaInfo(argv[2]);
    if (!info.has_value()) {
        LOG(ERROR) << "Failed to find entry for name '" << argv[2] << "'";
        return EXIT_FAILURE;
    } else {
        LOG(INFO) << "Found, sending (fileid " << info->mediaId << ") to chat "
                  << chatId;
    }
    strncpy(data.filePath.data(), info->mediaId.c_str(), data.filePath.size());
    data.chat = chatId;
    data.fileType = TgBotSocket::data::FileType::TYPE_DOCUMENT;

    struct TgBotSocket::Packet pkt(TgBotSocket::Command::CMD_SEND_FILE_TO_CHAT_ID, data);
    getClientBackend()->writeAsClientToSocket(pkt.toSocketData());
}
