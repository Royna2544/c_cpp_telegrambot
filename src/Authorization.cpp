#include <Authorization.h>
#include <Database.h>

bool gAuthorized = true;

static bool AuthorizedId(const int64_t id, const bool permissive) {
#ifdef USE_DATABASE

    if (!permissive) {
        if (database::whitelist.exists(id)) return true;
        return id == database::db.maybeGetOwnerId().value_or(-1);
    } else {
        if (database::blacklist.exists(id)) return false;
        return true;
    }
#else
    return permissive ? true : ownerid == id;
#endif
}
bool Authorized(const Message::Ptr &message, const bool nonuserallowed, const bool permissive) {
    if (!gAuthorized) return false;
    if (std::time(0) - message->date > 60) return false;

    return message->from ? AuthorizedId(message->from->id, permissive)
                         : nonuserallowed;
}
