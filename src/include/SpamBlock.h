#pragma once

#include "Types.h"
#include <NamespaceImport.h>

#ifdef SOCKET_CONNECTION
#include <socket/TgBotSocket.h>
#endif

#include <map>
#include <utility>

#ifdef SOCKET_CONNECTION
using namespace TgBotCommandData;
extern CtrlSpamBlock gSpamBlockCfg;
#endif

using ChatHandle = std::map<UserId, std::vector<Message::Ptr>>;
using UserType = std::pair<ChatId, std::vector<Message::Ptr>>;
using SpamMapT = std::map<UserId, std::vector<Message::Ptr>>;

void spamBlocker(const Bot& bot, const Message::Ptr& message);
