#pragma once

#include <Types.h>
#include <tgbot/types/Message.h>

#include <chrono>
#include "InstanceClassBase.hpp"

using TgBot::Message;

class AuthContext : public InstanceClassBase<AuthContext> {
   public:
    enum Flags : unsigned int {
        REQUIRE_USER = 0x1,  // If set, don't allow non-users
                             // e.g. channels, groups...
        PERMISSIVE = 0x2,    // If set, allow only for normal users (nonetheless
                             // of this flag, it excludes blacklist)
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
    [[nodiscard]] bool isAuthorized(const Message::Ptr& message,
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
constexpr std::chrono::seconds kErrorMaxDuration = std::chrono::seconds(30);
static_assert(kMaxTimestampDelay > kErrorRecoveryDelay);
