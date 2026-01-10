#pragma once

#include <optional>
#include <string>

namespace api::types {

/**
 * @brief This object represents type of a poll, which is allowed to be created
 * and sent when the corresponding button is pressed.
 */
struct KeyboardButtonPollType {
    /**
     * @brief Optional. If quiz is passed, the user will be allowed to create
     * only polls in the quiz mode. If regular is passed, only regular polls
     * will be allowed. Otherwise, the user will be allowed to create a poll of
     * any type.
     */
    std::optional<std::string> type;
};

}  // namespace api::types