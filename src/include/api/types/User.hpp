#pragma once

#include "forwards.hpp"

namespace tgbot_api {

/**
 * @brief This object represents a Telegram user or bot.
 */
class User {
   public:
    using Ptr = User*;

    /**
     * @brief Unique identifier for this user or bot.
     */
    UserId id{};

    /**
     * @brief True, if this user is a bot
     */
    bool isBot{false};

    /**
     * @brief User's or bot's first name
     */
    std::string firstName;

    /**
     * @brief Optional. User's or bot's last name
     */
    std::optional<std::string> lastName;

    /**
     * @brief Optional. User's or bot's username
     */
    std::optional<std::string> username;

    /**
     * @brief Optional. IETF language tag of the user's language
     */
    std::optional<std::string> languageCode;

    /**
     * @brief Optional. True, if this user is a Telegram Premium user
     */
    std::optional<bool> isPremium;

    /**
     * @brief Optional. True, if this user added the bot to the attachment menu
     */
    std::optional<bool> addedToAttachmentMenu;

    /**
     * @brief Optional. True, if the bot can be invited to groups.
     */
    std::optional<bool> canJoinGroups;

    /**
     * @brief Optional. True, if privacy mode is disabled for the bot.
     */
    std::optional<bool> canReadAllGroupMessages;

    /**
     * @brief Optional. True, if the bot supports inline queries.
     */
    std::optional<bool> supportsInlineQueries;

    /**
     * @brief Optional. True, if the bot can be connected to a Telegram Business
     * account.
     */
    std::optional<bool> canConnectToBusiness;
};

}  // namespace tgbot_api
