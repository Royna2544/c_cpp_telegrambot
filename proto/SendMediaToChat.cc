#include <Types.h>
#include <absl/log/initialize.h>
#include <absl/log/log.h>
#include <socket/TgBotSocket.h>
#include <impl/bot/ClientBackend.hpp>

#include <DatabaseBot.hpp>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include "SocketData.hpp"

[[noreturn]] static void usage(const char* argv0, const int exitCode) {
    std::cerr << "Usage: " << argv0 << " <chatId> <name stored in DB>"
              << std::endl;
    exit(exitCode);
}

int main(int argc, char* const* argv) {
    ChatId chatId = 0;
    TgBotCommandData::SendFileToChatId data = {};
    const auto _usage = [capture0 = argv[0]](auto&& PH1) {
        usage(capture0, std::forward<decltype(PH1)>(PH1));
    };
    auto backend = DefaultDatabase();

    absl::InitializeLog();
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
    strncpy(data.filepath, info->mediaId.c_str(), sizeof(data.filepath) - 1);
    data.id = chatId;
    data.type = TYPE_DOCUMENT;

    struct TgBotCommandPacket pkt(CMD_SEND_FILE_TO_CHAT_ID, data);
    getClientBackend()->writeAsClientToSocket(pkt.toSocketData());
}
