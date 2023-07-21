#pragma once

#include <tgbot/types/Message.h>

#include "NamespaceImport.h"

extern bool gAuthorized;
bool Authorized(const Message::Ptr &message,
                const bool nonuserallowed = false,
                const bool permissive = false);

#define ENFORCE_AUTHORIZED \
    if (!Authorized(message)) return
#define PERMISSIVE_AUTHORIZED \
    if (!Authorized(message, true, true)) return
