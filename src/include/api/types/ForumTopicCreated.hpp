#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace api::types {

/**
 * @brief This object represents a service message about a new forum topic
 * created in the chat.
 */
struct ForumTopicCreated {
    /**
     * @brief Name of the topic
     */
    std::string name;

    /**
     * @brief Color of the topic icon in RGB format
     */
    std::int32_t iconColor;

    /**
     * @brief Optional. Unique identifier of the custom emoji shown as the topic
     * icon
     */
    std::optional<std::string> iconCustomEmojiId;
};

}  // namespace api::types