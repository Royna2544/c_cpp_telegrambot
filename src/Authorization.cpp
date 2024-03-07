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
            bool ret = false;
            if (const auto whitelist = DBWrapper.whitelist; whitelist)
                ret |= whitelist->exists(id);
            ret |= id == database::DBWrapper.maybeGetOwnerId();
            return ret;
        }
    } else {
        return !(flags & AuthorizeFlags::REQUIRE_USER);
    }
}
