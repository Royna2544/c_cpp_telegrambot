package com.royna.tgbotclient.net.data

enum class Command(val value: Int) {
    CMD_INVALID(0),
    CMD_WRITE_MSG_TO_CHAT_ID(1),
    CMD_CTRL_SPAMBLOCK(2),
    CMD_OBSERVE_CHAT_ID(3),
    CMD_SEND_FILE_TO_CHAT_ID(4),
    CMD_OBSERVE_ALL_CHATS(5),
    CMD_GET_UPTIME(6),
    CMD_TRANSFER_FILE(7),
    CMD_TRANSFER_FILE_REQUEST(8),

    // Below are internal commands
    CMD_GET_UPTIME_CALLBACK(100),
    CMD_GENERIC_ACK(101),
    CMD_OPEN_SESSION(102),
    CMD_OPEN_SESSION_ACK(103),
    CMD_CLOSE_SESSION(104)
};