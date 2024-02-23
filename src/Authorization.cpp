#include <Authorization.h>
#include <Database.h>
#include <Types.h>

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
    if (!gAuthorized) return false;
    if (std::time(nullptr) - message->date > 60) return false;

    if (message->from) {
        return AuthorizedId(message->from->id, flags & AuthorizeFlags::PERMISSIVE);
    } else {
        return !(flags & AuthorizeFlags::REQUIRE_USER);
    }
}
