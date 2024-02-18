#pragma once

#include <NamespaceImport.h>

#include "Types.h"

#ifdef SOCKET_CONNECTION
#include <socket/TgBotSocket.h>
#endif

#include <atomic>
#include <map>
#include <memory>
#include <mutex>

#ifdef SOCKET_CONNECTION
using namespace TgBotCommandData;
extern CtrlSpamBlock gSpamBlockCfg;
#endif

using TgBot::Chat;
using TgBot::User;
using ChatHandle = std::map<User::Ptr, std::vector<Message::Ptr>>;

struct SpamBlockBuffer {
    std::map<Chat::Ptr, ChatHandle> buffer;
    std::map<Chat::Ptr, int> buffer_sub;
    std::mutex m;  // Protect buffer, buffer_sub
    std::atomic_bool kRun;
    std::thread kThreadP;
};

void spamBlocker(const Bot& bot, const Message::Ptr& message, std::shared_ptr<SpamBlockBuffer> buf);
