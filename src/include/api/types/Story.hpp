#pragma once

#include <api/types/Chat.hpp>
#include <cstdint>

namespace api::types {

/**
 * @brief This object represents a story.
 */
struct Story {
    /**
     * @brief Chat that posted the story
     */
    Chat chat;

    /**
     * @brief Unique identifier for the story in the chat
     */
    std::int32_t id;
};

}