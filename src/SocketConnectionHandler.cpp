#include <ChatObserver.h>
#include <SocketConnectionHandler.h>
#include <SpamBlock.h>
#include <utils/libutils.h>

void socketConnectionHandler(const Bot& bot, struct TgBotConnection conn) {
    auto _data = conn.data;
    switch (conn.cmd) {
        case CMD_WRITE_MSG_TO_CHAT_ID:
            try {
                bot.getApi().sendMessage(_data.data_1.to, _data.data_1.msg);
            } catch (const TgBot::TgException& e) {
                LOG_E("Exception at handler, %s", e.what());
            }
            break;
        case CMD_CTRL_SPAMBLOCK:
            gSpamBlockCfg = _data.data_3;
            break;
        case CMD_OBSERVE_CHAT_ID: {
            auto it = std::find(gObservedChatIds.begin(), gObservedChatIds.end(), _data.data_4.id);
            bool observe = _data.data_4.observe;
            if (it == gObservedChatIds.end()) {
                if (observe) {
                    LOG_D("Adding chat to observer");
                    gObservedChatIds.push_back(_data.data_4.id);
                } else {
                    LOG_W("Trying to quit observing chatid which wasn't being observed!");
                }
            } else {
                if (observe) {
                    LOG_W("Trying to observe chatid which was already being observed!");
                } else {
                    LOG_D("Removing chat from observer");
                    gObservedChatIds.erase(it);
                }
            }
        } break;
        default:
            LOG_E("Unexpected cmd: %s", toStr(conn.cmd).c_str());
            break;
    };
}
