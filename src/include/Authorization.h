#pragma once

#include <tgbot/types/Message.h>

#include <Types.h>
#include "NamespaceImport.h"

// A global 'authorized' bool object that controls all commands
// that are sent to the bot
extern bool gAuthorized;

/**
 * Authorized - controls the command policy of the bot
 *
 * Evalutes blacklist, whitelist to determine if this message is appropriate
 * to the policy passed as parameters.
 *
 * @param message Message ptr to evaluate
 * @param nonuserallowed Allow non-users, e.g. channels, groups...
 * @param permissive Whether it should be allowed to normal users (excludes blacklist)
 * @return true if the message is 'authorized' to process, otherwise false
 * @see Database.cpp
 */
bool Authorized(const Message::Ptr &message,
                const bool nonuserallowed = false,
                const bool permissive = false);
