#pragma once

#include <tgbot/types/Message.h>

#include <Types.h>
#include "NamespaceImport.h"

extern bool gAuthorized;
bool Authorized(const Message::Ptr &message,
                const bool nonuserallowed = false,
                const bool permissive = false);

static inline const UserId ownerid = 1185607882;
