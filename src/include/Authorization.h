#pragma once

#include <TgBotPPImpl_shared_depsExports.h>
#include <Types.h>
#include <tgbot/types/Message.h>

#include <chrono>

#include "InstanceClassBase.hpp"

using TgBot::Message;

class TgBotPPImpl_shared_deps_API AuthContext
    : public InstanceClassBase<AuthContext> {
   public:
    enum Flags : unsigned int {
        REQUIRE_USER = 0x1,  // If set, don't allow non-users
                             // e.g. channels, groups...
        PERMISSIVE = 0x2,    // If set, allow only for normal users (nonetheless
                             // of this flag, it excludes blacklist)
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
     * @param message The Telegram message.
     * @param flags A bitwise combination of the AuthContext::Flags values.
     * @return True if the message is authorized, false otherwise.
     */
    [[nodiscard]] Result isAuthorized(const Message::Ptr& message,
                                      const unsigned flags) const;

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
