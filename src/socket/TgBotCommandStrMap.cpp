#include "TgBotSocket.h"

std::unordered_map<TgBotCommand, std::string> kTgBotCommandStrMap = {
    ENUM_STR(CMD_WRITE_MSG_TO_CHAT_ID),
    ENUM_STR(CMD_EXIT),
    ENUM_STR(CMD_CTRL_SPAMBLOCK),
    ENUM_STR(CMD_MAX),
};
