#include <Authorization.h>
#include <Types.h>
#include <absl/log/log.h>

#include <InstanceClassBase.hpp>
#include <database/DatabaseBase.hpp>
#include <database/bot/TgBotDatabaseImpl.hpp>

DECLARE_CLASS_INST(AuthContext);

template <DatabaseBase::ListType type>
bool isInList(const UserId user) {
    const auto database = TgBotDatabaseImpl::getInstance();
    switch (database->checkUserInList(type, user)) {
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

AuthContext::Result AuthContext::isAuthorized(const Message::Ptr& message,
                                              const unsigned flags) const {
    const auto database = TgBotDatabaseImpl::getInstance();
    if (!authorized || !isMessageUnderTimeLimit(message)) {
        Result::Reason reason{};
        if (!authorized) {
            reason = Result::Reason::GLOBAL_FLAG_OFF;
        }
        if (!isMessageUnderTimeLimit(message)) {
            reason = Result::Reason::MESSAGE_TOO_OLD;
        }
        return {false, reason};
    }

    if (message->from) {
        const UserId id = message->from->id;
        if ((flags & Flags::PERMISSIVE) != 0) {
            bool isInBlacklist = isInList<DatabaseBase::ListType::BLACKLIST>(id);
            return {!isInBlacklist, Result::Reason::BLACKLISTED_USER};
        } else {
            bool ret = isInList<DatabaseBase::ListType::WHITELIST>(id);
            bool ret2 = id == database->getOwnerUserId();
            return {ret || ret2, Result::Reason::NOT_IN_WHITELIST};
        }
    } else {
        bool ignore = (flags & Flags::REQUIRE_USER) == Flags::REQUIRE_USER;
        return {!ignore, Result::Reason::REQUIRES_USER};
    }
}

bool AuthContext::isMessageUnderTimeLimit(const Message::Ptr& msg) noexcept {
    const auto MessageTp = std::chrono::system_clock::from_time_t(msg->date);
    const auto CurrentTp = std::chrono::system_clock::now();
    return (CurrentTp - MessageTp) <= kMaxTimestampDelay;
}
