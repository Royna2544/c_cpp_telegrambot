#pragma once

#include <NamespaceImport.h>

#include "Types.h"

#ifdef SOCKET_CONNECTION
#include <socket/TgBotSocket.h>
#endif

#include <map>
#include <utility>

#ifdef SOCKET_CONNECTION
using namespace TgBotCommandData;
extern CtrlSpamBlock gSpamBlockCfg;
#endif

void spamBlocker(const Bot& bot, const Message::Ptr& message);
