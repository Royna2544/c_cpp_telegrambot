#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace api::types {

/**
 * @brief This object represents a service message about an edited forum topic.
 */
struct ForumTopicEdited {
    /**
     * @brief Optional. New name of the topic, if it was edited
     */
    std::optional<std::string> name;

    /**
     * @brief Optional. New identifier of the custom emoji shown as the topic
     * icon, if it was edited; an empty string if the icon was removed
     */
    std::optional<std::string> iconCustomEmojiId;
};

}  // namespace api::types