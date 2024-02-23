#include <Authorization.h>
#include <Database.h>
#include <Types.h>
#include <chrono>

bool gAuthorized = true;

static bool AuthorizedId(const UserId id, const bool permissive) {
    if (!permissive) {
        if (database::whitelist.exists(id)) return true;
        return id == database::db.maybeGetOwnerId();
    } else {
        return !database::blacklist.exists(id);
    }
}
bool Authorized(const Message::Ptr &message, const int flags) {
    if (!gAuthorized || !isMessageUnderTimeLimit(message)) return false;

    if (message->from) {
        return AuthorizedId(message->from->id, flags & AuthorizeFlags::PERMISSIVE);
    } else {
        return !(flags & AuthorizeFlags::REQUIRE_USER);
    }
}
