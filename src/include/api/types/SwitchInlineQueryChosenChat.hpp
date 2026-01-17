#pragma once

#include <optional>
#include <string>

namespace api::types {

/**
 * @brief This object represents an inline button that switches the current user
 * to inline mode in a chosen chat, with an optional default inline query.
 */
struct SwitchInlineQueryChosenChat {
    /**
     * @brief Optional. The default inline query to be inserted in the input
     * field. If left empty, only the bot's username will be inserted
     */
    std::optional<std::string> query;

    /**
     * @brief Optional. True, if private chats with users can be chosen
     */
    std::optional<bool> allowUserChats;

    /**
     * @brief Optional. True, if private chats with bots can be chosen
     */
    std::optional<bool> allowBotChats;

    /**
     * @brief Optional. True, if group and supergroup chats can be chosen
     */
    std::optional<bool> allowGroupChats;

    /**
     * @brief Optional. True, if channel chats can be chosen
     */
    std::optional<bool> allowChannelChats;
};

}  // namespace api::types