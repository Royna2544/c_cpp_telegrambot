#pragma once

#include <string>

namespace api::types {

/**
 * @brief This object represents a bot command.
 *
 * https://core.telegram.org/bots/api#botcommand
 */
struct BotCommand {
    /**
     * @brief command label.
     */
    std::string command;

    /**
     * @brief description label.
     */
    std::string description;
};

}  // namespace api::types