#include <Authorization.h>
#include <Database.h>
#include <Types.h>

bool gAuthorized = true;

using database::DBWrapper;

bool Authorized(const Message::Ptr &message, const int flags) {
    if (!gAuthorized || !isMessageUnderTimeLimit(message)) return false;

    if (message->from) {
        const UserId id = message->from->id;
        if (flags & AuthorizeFlags::PERMISSIVE) {
            if (const auto blacklist = DBWrapper.blacklist; blacklist)
                return !blacklist->exists(id);
            return true;
        } else {
            if (const auto whitelist = DBWrapper.whitelist; whitelist)
                return whitelist->exists(id);
            return id == database::DBWrapper.maybeGetOwnerId();
        }
    } else {
        return !(flags & AuthorizeFlags::REQUIRE_USER);
    }
}
