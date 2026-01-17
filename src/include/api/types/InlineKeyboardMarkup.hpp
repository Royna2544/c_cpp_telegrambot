#pragma once

#include <api/types/InlineKeyboardButton.hpp>
#include <vector>

namespace api::types {

/**
 * @brief This object represents an [inline
 * keyboard](https://core.telegram.org/bots/features#inline-keyboards) that
 * appears right next to the message it belongs to.
 */
struct InlineKeyboardMarkup {
    /**
     * @brief Array of button rows, each represented by an Array of
     * InlineKeyboardButton objects
     */
    std::vector<std::vector<InlineKeyboardButton>> inlineKeyboard;
};

}  // namespace api::types
