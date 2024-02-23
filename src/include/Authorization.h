#pragma once

#include <chrono>

#include <tgbot/types/Message.h>

#include <Types.h>
#include "NamespaceImport.h"

// A global 'authorized' bool object that controls all commands
// that are sent to the bot
extern bool gAuthorized;

// Flags used in Authorized (see below)
enum AuthorizeFlags : int {
    REQUIRE_USER = 0x1, // If set, don't allow non-users, e.g. channels, groups...
    PERMISSIVE = 0x2,   // If set, allow only for normal users (nonetheless of this flag, it excludes blacklist)
};

constexpr std::chrono::seconds kMaxTimestampDelay = std::chrono::seconds(5);

/**
 * Authorized - controls the command policy of the bot
 *
 * Evalutes blacklist, whitelist to determine if this message is appropriate
 * to the policy passed as parameters.
 *
 * @param message Message ptr to evaluate
 * @param flags Flags for authorization query
 * @return true if the message is 'authorized' to process, otherwise false
 * @see Database.cpp
 */
bool Authorized(const Message::Ptr &message, const int flags);

inline bool isMessageUnderTimeLimit(const Message::Ptr& msg) {   
    const auto MessageTp = std::chrono::system_clock::from_time_t(msg->date);
    const auto CurrentTp = std::chrono::system_clock::now();
    return (CurrentTp - MessageTp) <= kMaxTimestampDelay;
}