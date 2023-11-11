#include <SocketConnectionHandler.h>
#include <SpamBlock.h>
#include <utils/libutils.h>

void socketConnectionHandler(const Bot& bot, struct TgBotConnection conn) {
    auto _data = conn.data;
    switch (conn.cmd) {
        case CMD_WRITE_MSG_TO_CHAT_ID:
            bot.getApi().sendMessage(_data.data_1.to, _data.data_1.msg);
            break;
        case CMD_CTRL_SPAMBLOCK:
            gSpamBlockCfg = _data.data_3;
            break;
        default:
            LOG_E("Unexpected cmd: %s", toStr(conn.cmd).c_str());
            break;
    };
}
