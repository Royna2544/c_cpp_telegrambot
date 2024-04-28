#include <Authorization.h>
#include <Types.h>

#include "DatabaseBot.hpp"
#include "InstanceClassBase.hpp"
#include "database/DatabaseBase.hpp"

bool gAuthorized = true;

DECLARE_CLASS_INST(DefaultBotDatabase);

inline DefaultDatabase& getAuthorizationDB() {
    try {
        return DefaultBotDatabase::getInstance();
    } catch (const std::runtime_error& e) {
        // For testing purposes...
        static DefaultDatabase base;
        return base;
    }
}

template <DatabaseBase::ListType type>
bool isInList(const DefaultDatabase& database, const UserId user) {
    switch (database.checkUserInList(type, user)) {
        case DatabaseBase::ListResult::OK:
            return true;
        case DatabaseBase::ListResult::BACKEND_ERROR:
            LOG(ERROR) << "Backend error in checkUserInList()";
            [[fallthrough]];
        case DatabaseBase::ListResult::ALREADY_IN_OTHER_LIST:
        case DatabaseBase::ListResult::NOT_IN_LIST:
            return false;
        default:
            LOG(ERROR) << "Unknown error in checkUserInList()";
            return false;
    }
}

bool Authorized(const Message::Ptr& message, const int flags) {
    static const auto& backend = getAuthorizationDB();

    if (!gAuthorized || !isMessageUnderTimeLimit(message)) {
        return false;
    }

    if (message->from) {
        const UserId id = message->from->id;
        if ((flags & AuthorizeFlags::PERMISSIVE) != 0) {
            return !isInList<DatabaseBase::ListType::BLACKLIST>(backend, id);
        } else {
            bool ret = isInList<DatabaseBase::ListType::WHITELIST>(backend, id);
            ret |= id == backend.getOwnerUserId();
            return ret;
        }
    } else {
        return !(flags & AuthorizeFlags::REQUIRE_USER);
    }
}
