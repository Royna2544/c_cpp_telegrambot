#include <absl/log/log.h>
#include <api/typedefs.h>

#include <api/AuthContext.hpp>

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
                                              const AccessLevel flags) const {
    if (isUnderTimeLimit(message)) {
        return isAuthorized(message->from, flags);
    } else {
        return {false, Result::Reason::MessageTooOld};
    }
}

AuthContext::Result AuthContext::isAuthorized(const User::Ptr& user,
                                              const AccessLevel flags) const {
    std::optional<UserId> id;
    if (user && !user->isBot) {
        // Obtain id
        id = user->id;
    }
    // If user is bot, quickly send off
    // if acl
    if (!id) {
        if (flags > AccessLevel::Unprotected) {
            return {false, Result::Reason::UserIsBot};
        } else {
            return {true, Result::Reason::Ok};
        }
    }

    bool isInBlacklist = isInList(DatabaseBase::ListType::BLACKLIST, *id);
    bool isInWhitelist = isInList(DatabaseBase::ListType::WHITELIST, *id);
    bool isOwner = *id == _impl->getOwnerUserId();
    switch (flags) {
        case AccessLevel::User: {
            return {!isInBlacklist, Result::Reason::ForbiddenUser};
        }
        case AccessLevel::AdminUser: {
            return {isInWhitelist || isOwner,
                    isInBlacklist ? Result::Reason::ForbiddenUser
                                  : Result::Reason::PermissionDenied};
        }
        case AccessLevel::Unprotected: {
            return {!!id, Result::Reason::UserIsBot};
        }
        default:
            LOG(ERROR) << "Should not reach";
            return {false, Result::Reason::Unknown};
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
