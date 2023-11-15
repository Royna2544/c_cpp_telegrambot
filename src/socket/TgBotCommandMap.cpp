#include "TgBotSocket.h"

const auto kTgBotCommandStrMap = make_array<ConstArrayElem<TgBotCommand, std::string>, CMD_MAX - 1>(
    ENUM_STR(CMD_WRITE_MSG_TO_CHAT_ID),
    ENUM_STR(CMD_CTRL_SPAMBLOCK),
    ENUM_STR(CMD_OBSERVE_CHAT_ID));

const auto kTgBotCommandArgsCount = make_array<ConstArrayElem<TgBotCommand, int>, CMD_MAX - 1>(
    ARGUMENT_SIZE(CMD_WRITE_MSG_TO_CHAT_ID, 2),  // chatid, msg
    ARGUMENT_SIZE(CMD_CTRL_SPAMBLOCK, 1),        // policy
    ARGUMENT_SIZE(CMD_OBSERVE_CHAT_ID, 2)        // chatid, policy
);
