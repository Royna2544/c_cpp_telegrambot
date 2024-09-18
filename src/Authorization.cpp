#include <Authorization.h>
#include <Types.h>

#include <InstanceClassBase.hpp>
#include <database/DatabaseBase.hpp>

#include "absl/log/log.h"
#include "database/bot/TgBotDatabaseImpl.hpp"

// #define AUTHORIZATION_DEBUG

#ifdef AUTHORIZATION_DEBUG
#include <iomanip>
#include <mutex>
#endif

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
#ifdef AUTHORIZATION_DEBUG
    static std::mutex authStdoutLock;
#endif
    if (!authorized || !isMessageUnderTimeLimit(message)) {
#ifdef AUTHORIZATION_DEBUG
        const std::lock_guard<std::mutex> _(authStdoutLock);
        DLOG(INFO) << "Not authorized: message text: "
                   << std::quoted(message->text);
        if (!authorized) {
            DLOG(INFO) << "why?: Global authorization flag is off";
        }
        if (!isMessageUnderTimeLimit(message)) {
            DLOG(INFO) << "why?: Message is too old";
        }
#endif
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
            bool isInBlacklist = false;
#ifdef AUTHORIZATION_DEBUG
            const std::lock_guard<std::mutex> _(authStdoutLock);

            DLOG(INFO) << "Checking if user id in blacklist";
#endif
            isInBlacklist = isInList<DatabaseBase::ListType::BLACKLIST>(id);
#ifdef AUTHORIZATION_DEBUG
            DLOG(INFO) << "User id in blacklist: " << std::boolalpha
                       << isInBlacklist;
#endif
            return {!isInBlacklist, Result::Reason::BLACKLISTED_USER};
        } else {
            bool ret = isInList<DatabaseBase::ListType::WHITELIST>(id);
            bool ret2 = id == database->getOwnerUserId();
#ifdef AUTHORIZATION_DEBUG
            const std::lock_guard<std::mutex> _(authStdoutLock);

            DLOG(INFO) << "Checking if user id in whitelist: " << std::boolalpha
                       << ret;
            DLOG(INFO) << "User is owner of this bot: " << std::boolalpha
                       << ret2;
#endif
            return {ret || ret2, Result::Reason::NOT_IN_WHITELIST};
        }
    } else {
        bool ignore = (flags & Flags::REQUIRE_USER) == Flags::REQUIRE_USER;
#ifdef AUTHORIZATION_DEBUG
        DLOG(INFO) << "Should ignore the message: " << std::boolalpha << ignore;
#endif
        return {!ignore, Result::Reason::REQUIRES_USER};
    }
}

bool AuthContext::isMessageUnderTimeLimit(const Message::Ptr& msg) noexcept {
    const auto MessageTp = std::chrono::system_clock::from_time_t(msg->date);
    const auto CurrentTp = std::chrono::system_clock::now();
    return (CurrentTp - MessageTp) <= kMaxTimestampDelay;
}
