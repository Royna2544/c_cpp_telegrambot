#include <functional>
#include <string>

#define SOCKET_PATH "/tmp/tgbot_sock"

enum TgBotCommand {
    CMD_WRITE_MSG_TO_CHAT_ID,
    CMD_EXIT,
    CMD_MAX,
};

namespace TgBotCommandData {
struct WriteMsgToChatId {
    int64_t to; // To, chatid in numbers
    char msg[2048]; // Msg to send
};
}  // namespace TgBotCommandData

union TgBotCommandUnion {
    TgBotCommandData::WriteMsgToChatId data_1;
    int data_2; // unused
};

struct TgBotConnection {
    TgBotCommand cmd;
    union TgBotCommandUnion data;
};

using listener_callback_t = std::function<void(struct TgBotConnection)>;

void startListening(listener_callback_t cb);
void writeToSocket(struct TgBotConnection cn);
