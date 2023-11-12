#include <Authorization.h>
#include <Database.h>
#include <Types.h>

bool gAuthorized = true;

static bool AuthorizedId(const UserId id, const bool permissive) {
    if (!permissive) {
        if (database::whitelist.exists(id)) return true;
        return id == database::db.maybeGetOwnerId().value_or(-1);
    } else {
        return !database::blacklist.exists(id);
    }
}
bool Authorized(const Message::Ptr &message, const bool nonuserallowed, const bool permissive) {
    if (!gAuthorized) return false;
    if (std::time(nullptr) - message->date > 60) return false;

    return message->from ? AuthorizedId(message->from->id, permissive)
                         : nonuserallowed;
}
