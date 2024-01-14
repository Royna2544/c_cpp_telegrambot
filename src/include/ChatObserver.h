#pragma once

#include <NamespaceImport.h>
#include <Types.h>

#include <vector>

// Global ChatId list to observe
extern std::vector<ChatId> gObservedChatIds;
extern bool gObserveAllChats;

/**
 * processObservers - Process a msg and log the chat content
 *
 * The chat msgs will be logged if the ChatId is being 'observed'
 * @param msg message object to observe
 */
void processObservers(const Message::Ptr& msg);
