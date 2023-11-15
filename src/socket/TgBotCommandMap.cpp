#include "TgBotSocket.h"

const ConstArray<TgBotCommand, std::string, CMD_MAX - 1> kTgBotCommandStrMap = {
    ENUM_STR(CMD_WRITE_MSG_TO_CHAT_ID),
    ENUM_STR(CMD_CTRL_SPAMBLOCK),
    ENUM_STR(CMD_OBSERVE_CHAT_ID),
};

const ConstArray<TgBotCommand, int, CMD_MAX - 1> kTgBotCommandArgsCount = {
    ARGUMENT_SIZE(CMD_WRITE_MSG_TO_CHAT_ID, 2),  // chatid, msg
    ARGUMENT_SIZE(CMD_CTRL_SPAMBLOCK, 1),        // policy
    ARGUMENT_SIZE(CMD_OBSERVE_CHAT_ID, 2),       // chatid, policy
};

static_assert(kTgBotCommandStrMap.size() == kTgBotCommandArgsCount.size(), "Command map size is different");
static_assert(kTgBotCommandStrMap.size() == CMD_MAX - 1, "Add all members in TgBotCommand enum");
static_assert(kTgBotCommandArgsCount.size() == CMD_MAX - 1, "Add all members in TgBotCommand enum");
