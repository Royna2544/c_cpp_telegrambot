#include <Authorization.h>
#include <Database.h>
#include <Types.h>

bool gAuthorized = true;

namespace database {
inline DatabaseWrapperImpl& getDB() {
    try {
        return DatabaseWrapperBotImplObj::getInstance();
    } catch (const std::runtime_error& e) {
        LOG(ERROR) << e.what();
        return DatabaseWrapperImplObj::getInstance();
    }
}
};  // namespace database

bool Authorized(const Message::Ptr& message, const int flags) {
    static auto& DBWrapper = database::getDB();

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
            ret |= id == DBWrapper.maybeGetOwnerId();
            return ret;
        }
    } else {
        return !(flags & AuthorizeFlags::REQUIRE_USER);
    }
}
