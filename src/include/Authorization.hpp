#pragma once

#include <Types.h>
#include <tgbot/types/Message.h>

#include <chrono>

#include "InstanceClassBase.hpp"

using TgBot::Message;
using TgBot::User;

class AuthContext : public InstanceClassBase<AuthContext> {
   public:
    enum class Flags {
        None = 0,
        // If set, don't allow non-users e.g. channels, groups..
        REQUIRE_USER = 0x1,
        // If set, allow only for normal users (nonetheless of this flag, it
        // excludes blacklist)
        PERMISSIVE = 0x2,
    };

    // Holds result of isAuthorized() function
    struct Result {
        // Was it authorized?
        bool authorized{};
        // Reason for authorization failure.
        enum class Reason {
            UNKNOWN,
            // Used if authorization succeeded, only if.
            OK,
            // Message timestamp is over the limit towards the past.
            MESSAGE_TOO_OLD,
            // Blacklisted user
            BLACKLISTED_USER,
            // Global authentication is disabled
            GLOBAL_FLAG_OFF,
            // Not allowed, i.e. User is not in whitelist or owner
            // but tried to run enforced command.
            NOT_IN_WHITELIST,
            // Requires a user, but not provided.
            REQUIRES_USER
        } reason{};

        operator bool() const { return authorized; }

        // Constructor for Result.
        Result(const bool _cond, const Reason _reason) {
            if (_cond) {
                authorized = true;
                reason = Reason::OK;
            } else {
                authorized = false;
                reason = _reason;
            }
        }
    };

    // Set/Get the global 'authorized' bool object.
    [[nodiscard]] bool& isAuthorized() noexcept { return authorized; }

    /**
     * @brief Checks if the message is authorized to be processed by the bot.
     *
     * This function checks if the message is from a user, and if the user is
     * authorized to use the bot. The authorization is determined by the
     * blacklists and whitelists in the database.
     *
     * @param user The Telegram user.
     * @param flags A bitwise combination of the AuthContext::Flags values.
     * @return True if the message is authorized, false otherwise.
     */

    [[nodiscard]] Result isAuthorized(const User::Ptr& user,
                                      const Flags flags) const;

    // Overload taking a message instead of user
    [[nodiscard]] Result isAuthorized(const Message::Ptr& message,
                                      const Flags flags) const;

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
    static bool isMessageUnderTimeLimit(const Message::Ptr& msg) noexcept;

   private:
    bool authorized = true;
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
        CASE_STR(UNKNOWN);
        CASE_STR(OK);
        CASE_STR(MESSAGE_TOO_OLD);
        CASE_STR(BLACKLISTED_USER);
        CASE_STR(GLOBAL_FLAG_OFF);
        CASE_STR(NOT_IN_WHITELIST);
        CASE_STR(REQUIRES_USER);
    };

#undef CASE_STR
    return self;
}

constexpr inline AuthContext::Flags operator|=(AuthContext::Flags& lhs,
                                               const AuthContext::Flags& rhs) {
    return lhs = static_cast<AuthContext::Flags>(static_cast<int>(lhs) |
                                                 static_cast<int>(rhs));
}

constexpr inline bool operator&(const AuthContext::Flags& lhs,
                                const AuthContext::Flags& rhs) {
    return static_cast<bool>(static_cast<int>(lhs) & static_cast<int>(rhs));
}