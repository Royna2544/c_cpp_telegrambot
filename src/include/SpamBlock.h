#pragma once

#include "Types.h"
#include <NamespaceImport.h>

#include <map>
#include <utility>

using ChatHandle = std::map<UserId, std::vector<Message::Ptr>>;
using UserType = std::pair<ChatId, std::vector<Message::Ptr>>;
using SpamMapT = std::map<UserId, std::vector<Message::Ptr>>;

void spamBlocker(const Message::Ptr& msg);
