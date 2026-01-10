#pragma once

#include <api/types/User.hpp>
#include <api/types/Message.hpp>

#include <chrono>
#include <database/DatabaseBase.hpp>

#include "trivial_helpers/fruit_inject.hpp"

class AuthContext {
   public:
    APPLE_EXPLICIT_INJECT(AuthContext(DatabaseBase* database))
        : _impl(database) {}

    // AccessLevel
    // Unprotected: Anything including bot messages
    // User: Requires a valid user (Human) excl. blacklist
    // AdminUser: Owner or whitelisted user
    enum class AccessLevel {
        Unprotected,
        User,
        AdminUser,
    };

    // Holds result of isAuthorized() function
    struct Result {
        // Reason for authorization failure.
        enum class Reason {
            /// Default value. Should not occur unless explicitly set
            Unknown,
            /// Authorization succeeded.
            Ok,
            /// Message timestamp is too far in the past (expired).
            MessageTooOld,
            /// Sender is in the blacklist.
            ForbiddenUser,
            /// Sender lacks permission to run a privileged command.
            PermissionDenied,
            /// Sender is a bot, and bots are not allowed.
            UserIsBot
        };
        std::pair<bool, Reason> result;

        operator bool() const { return result.first; }

        // Constructor for Result.
        Result(const bool cond_, const Reason reason_) {
            if (cond_) {
                result = {true, Reason::Ok};
            } else {
                result = {false, reason_};
            }
        }
    };

    /**
     * @brief Checks if the message is authorized to be processed by the bot.
     *
     * This function checks if the message is from a user, and if the user is
     * authorized to use the bot. The authorization is determined by the
     * blacklists and whitelists in the database.
     *
     * @param user The Telegram user.
     * @param flags A bitwise combination of the AuthContext::AccessLevel
     * values.
     * @return True if the message is authorized, false otherwise.
     */

    [[nodiscard]] Result isAuthorized(
        const std::optional<api::types::User>& user,
        const AccessLevel flags = AccessLevel::Unprotected) const;

    // Overload taking a message instead of user
    [[nodiscard]] Result isAuthorized(
        const api::types::Message& message,
        const AccessLevel flags = AccessLevel::Unprotected) const;

    /**
     * @brief Checks if the message is within the allowed time limit.
     *
     * This function checks if the time difference between the current time and
     * the time of the received message is less than or equal to the maximum
     * allowed timestamp delay. If the time difference exceeds the maximum
     * allowed timestamp delay, the function returns false, indicating that the
     * message is not within the allowed time limit.
     *
     * @param message The Telegram message.
     * @return True if the message is within the allowed time limit, false
     * otherwise.
     */
    static bool isUnderTimeLimit(const api::types::Message& msg) noexcept;
    // Overload taking a time_t
    static bool isUnderTimeLimit(const time_t time) noexcept;

    [[nodiscard]] bool isInList(DatabaseBase::ListType type,
                                const api::types::User::id_type user) const;

   private:
    DatabaseBase* _impl;
};

constexpr std::chrono::seconds kMaxTimestampDelay = std::chrono::seconds(10);
constexpr std::chrono::seconds kErrorRecoveryDelay = std::chrono::seconds(7);
constexpr std::chrono::seconds kErrorMaxDuration = std::chrono::minutes(5);
static_assert(kMaxTimestampDelay > kErrorRecoveryDelay);

inline std::ostream& operator<<(std::ostream& self,
                                const AuthContext::Result::Reason& reason) {
#define CASE_STR(enum)                      \
    case AuthContext::Result::Reason::enum: \
        self << #enum;                      \
        break

    switch (reason) {
        CASE_STR(Unknown);
        CASE_STR(Ok);
        CASE_STR(MessageTooOld);
        CASE_STR(ForbiddenUser);
        CASE_STR(PermissionDenied);
        CASE_STR(UserIsBot);
    };

#undef CASE_STR
    return self;
}

inline std::ostream& operator<<(std::ostream& self,
                                const AuthContext::AccessLevel& level) {
#define CASE_STR(enum)                   \
    case AuthContext::AccessLevel::enum: \
        self << #enum;                   \
        break

    switch (level) {
        CASE_STR(Unprotected);
        CASE_STR(User);
        CASE_STR(AdminUser);
    };

#undef CASE_STR
    return self;
}
constexpr inline auto operator<=>(const AuthContext::AccessLevel lhs,
                                  const AuthContext::AccessLevel rhs) {
    return static_cast<int>(lhs) <=> static_cast<int>(rhs);
}
