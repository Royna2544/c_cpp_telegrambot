#include <api/typedefs.h>
#include <absl/log/log.h>

#include <api/Authorization.hpp>

bool AuthContext::isInList(DatabaseBase::ListType type,
                           const UserId user) const {
    switch (_impl->checkUserInList(type, user)) {
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
                                              const Flags flags) const {
    if (isUnderTimeLimit(message)) {
        return isAuthorized(message->from, flags);
    } else {
        return {false, Result::Reason::MESSAGE_TOO_OLD};
    }
}

AuthContext::Result AuthContext::isAuthorized(const User::Ptr& user,
                                              const Flags flags) const {
    if (!authorized) {
        return {false, Result::Reason::GLOBAL_FLAG_OFF};
    }

    if (user) {
        const UserId id = user->id;
        if (flags & Flags::PERMISSIVE) {
            bool isInBlacklist =
                isInList(DatabaseBase::ListType::BLACKLIST, id);
            return {!isInBlacklist, Result::Reason::BLACKLISTED_USER};
        } else {
            bool ret = isInList(DatabaseBase::ListType::WHITELIST, id);
            bool ret2 = id == _impl->getOwnerUserId();
            return {ret || ret2, Result::Reason::NOT_IN_WHITELIST};
        }
    } else {
        bool ignore = (flags & Flags::REQUIRE_USER);
        return {!ignore, Result::Reason::REQUIRES_USER};
    }
}

bool AuthContext::isUnderTimeLimit(const Message::Ptr& msg) noexcept {
    return isUnderTimeLimit(msg->date);
}

bool AuthContext::isUnderTimeLimit(const time_t time) noexcept {
    const auto MessageTp = std::chrono::system_clock::from_time_t(time);
    const auto CurrentTp = std::chrono::system_clock::now();
    return (CurrentTp - MessageTp) <= kMaxTimestampDelay;
}
