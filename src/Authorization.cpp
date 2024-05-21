#include <Authorization.h>
#include <Types.h>

#include <DatabaseBot.hpp>
#include <InstanceClassBase.hpp>
#include <database/DatabaseBase.hpp>
#include <memory>

#include "absl/log/log.h"

#ifdef AUTHORIZATION_DEBUG
#include <iomanip>
#include <mutex>
#endif

DECLARE_CLASS_INST(DefaultBotDatabase);
DECLARE_CLASS_INST(AuthContext);

template <DatabaseBase::ListType type>
bool isInList(const std::shared_ptr<DefaultDatabase> database,
              const UserId user) {
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

bool AuthContext::isAuthorized(const Message::Ptr& message,
                               const unsigned flags) const {
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
        return false;
    }

    if (message->from) {
        const UserId id = message->from->id;
        if ((flags & Flags::PERMISSIVE) != 0) {
            bool isInBlacklist = false;
#ifdef AUTHORIZATION_DEBUG
            const std::lock_guard<std::mutex> _(authStdoutLock);

            DLOG(INFO) << "Checking if user id in blacklist";
#endif
            isInBlacklist =
                isInList<DatabaseBase::ListType::BLACKLIST>(database, id);
#ifdef AUTHORIZATION_DEBUG
            DLOG(INFO) << "User id in blacklist: " << std::boolalpha
                       << isInBlacklist;
#endif
            return !isInBlacklist;
        } else {
            bool ret =
                isInList<DatabaseBase::ListType::WHITELIST>(database, id);
            bool ret2 = id == database->getOwnerUserId();
#ifdef AUTHORIZATION_DEBUG
            const std::lock_guard<std::mutex> _(authStdoutLock);

            DLOG(INFO) << "Checking if user id in whitelist: " << std::boolalpha
                       << ret;
            DLOG(INFO) << "User is owner of this bot: " << std::boolalpha
                       << ret2;
#endif
            return ret || ret2;
        }
    } else {
        bool ignore = (flags & Flags::REQUIRE_USER) == Flags::REQUIRE_USER;
#ifdef AUTHORIZATION_DEBUG
        DLOG(INFO) << "Should ignore the message: " << std::boolalpha << ignore;
#endif
        return !ignore;
    }
}

bool AuthContext::isMessageUnderTimeLimit(const Message::Ptr& msg) noexcept {
    const auto MessageTp = std::chrono::system_clock::from_time_t(msg->date);
    const auto CurrentTp = std::chrono::system_clock::now();
    return (CurrentTp - MessageTp) <= kMaxTimestampDelay;
}