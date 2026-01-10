#pragma once

#include <cstdint>
#include <string>
#include <optional>

namespace api::types {

 /**
 * @brief This object represents a Telegram user or bot.
 */
struct User {
    using id_type = std::int64_t;

    /**
     * @brief Unique identifier for this user or bot.
     *
     * This number may have more than 32 significant bits and some programming
     * languages may have difficulty/silent defects in interpreting it. But it
     * has at most 52 significant bits, so a 64-bit integer or double-precision
     * float type are safe for storing this identifier.
     */
    id_type id;

    /**
     * @brief True, if this user is a bot
     */
    bool isBot;

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
     * @brief Optional. [IETF language
     * tag](https://en.wikipedia.org/wiki/IETF_language_tag) of the user's
     * language
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
     *
     * Returned only in Api::getMe.
     */
    std::optional<bool> canJoinGroups;

    /**
     * @brief Optional. True, if [privacy
     * mode](https://core.telegram.org/bots/features#privacy-mode) is disabled
     * for the bot.
     *
     * Returned only in Api::getMe.
     */
    std::optional<bool> canReadAllGroupMessages;

    /**
     * @brief Optional. True, if the bot supports inline queries.
     *
     * Returned only in Api::getMe.
     */
    std::optional<bool> supportsInlineQueries;

    /**
     * @brief Optional. True, if the bot can be connected to a Telegram Business
     * account to receive its messages.
     *
     * Returned only in Api::getMe.
     */
    std::optional<bool> canConnectToBusiness;
};

}  // namespace api::types