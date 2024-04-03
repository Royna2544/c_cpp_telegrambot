#include <Database.h>
#include <Types.h>
#include <absl/log/initialize.h>
#include <absl/log/log.h>
#include <socket/TgBotSocket.h>

#include <cstdlib>
#include <cstring>
#include <string>

#include "socket/getter/SocketInterfaceGetter.hpp"

#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif

[[noreturn]] static void usage(const char* argv0, const int exitCode) {
    std::cerr << "Usage: " << argv0 << " <chatId> <name stored in DB>"
              << std::endl;
    exit(exitCode);
}

int main(int argc, char* const* argv) {
    ChatId chatId = 0;
    TgBotCommandData::SendFileToChatId data = {};
    const auto _usage = std::bind(usage, argv[0], std::placeholders::_1);
    auto& DBWrapper = database::DatabaseWrapperImplObj::getInstance();

    absl::InitializeLog();
    if (argc != 3) {
        _usage(EXIT_SUCCESS);
    }
    DBWrapper.load();
    try {
        chatId = std::stoll(argv[1]);
    } catch (...) {
        LOG(ERROR) << "Failed to convert '" << argv[1] << "' to ChatId";
        _usage(EXIT_FAILURE);
    }
    const auto mediaEntries = DBWrapper.protodb.mediatonames();
    std::optional<tgbot::proto::MediaToName> it;
    for (int i = 0; i < mediaEntries.size(); ++i) {
        const auto mediaEntriesIt = mediaEntries.Get(i);
        if (const auto mediaEntriesNames = mediaEntriesIt.names();
            mediaEntriesNames.size() > 0) {
            for (int j = 0; j < mediaEntriesNames.size(); ++j) {
                if (!strcasecmp(mediaEntriesNames.Get(j).c_str(), argv[2])) {
                    it = mediaEntriesIt;
                    break;
                }
            }
        }
    }
    if (!it) {
        LOG(ERROR) << "Failed to find entry for name '" << argv[2] << "'";
        return EXIT_FAILURE;
    } else {
        LOG(INFO) << "Found, sending (fileid " << it->telegrammediaid()
                  << ") to chat " << chatId;
    }
    strncpy(data.filepath, it->telegrammediaid().c_str(),
            sizeof(data.filepath) - 1);
    data.id = chatId;
    data.type = TYPE_DOCUMENT;

    struct TgBotConnection conn {};
    conn.cmd = CMD_SEND_FILE_TO_CHAT_ID;
    conn.data.data_5 = data;
    SocketInterfaceGetter::getForClient()->writeToSocket(conn);
}
