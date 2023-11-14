#pragma once

#include <vector>

#include <NamespaceImport.h>
#include <Types.h>

extern std::vector<ChatId> gObservedChatIds;

void processObservers(const Message::Ptr& msg);
