#pragma once

#include <tgbot/types/Chat.h>
#include <tgbot/types/User.h>

#include "SingleThreadCtrl.h"

#ifdef SOCKET_CONNECTION
#include <socket/TgBotSocket.h>
#endif

#include <map>
#include <mutex>

#ifdef SOCKET_CONNECTION
using namespace TgBotCommandData;
extern CtrlSpamBlock gSpamBlockCfg;
#endif

using TgBot::Chat;
using TgBot::User;
using ChatHandle = std::map<User::Ptr, std::vector<Message::Ptr>>;

struct SpamBlockManager : SingleThreadCtrl {
    SpamBlockManager() : SingleThreadCtrl() {}
    ~SpamBlockManager() override = default;

    void spamBlockerThreadFn(const Bot& bot);
    void run(const Bot &bot, const Message::Ptr &message);

 private:
    std::map<Chat::Ptr, ChatHandle> buffer;
    std::map<Chat::Ptr, int> buffer_sub;
    std::mutex m;  // Protect buffer, buffer_sub
};
