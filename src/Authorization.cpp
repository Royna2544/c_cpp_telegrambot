#include <Authorization.h>
#include <Database.h>
#include <Types.h>

bool gAuthorized = true;

bool Authorized(const Message::Ptr &message, const int flags) {
    if (!gAuthorized || !isMessageUnderTimeLimit(message)) return false;

    if (message->from) {
        const UserId id = message->from->id;
        if (flags & AuthorizeFlags::PERMISSIVE) {
            return !database::DBWrapper.blacklist->exists(id);
        } else {
            if (database::DBWrapper.whitelist->exists(id))
                return true;
            return id == database::DBWrapper.maybeGetOwnerId();
        }
    } else {
        return !(flags & AuthorizeFlags::REQUIRE_USER);
    }
}
