#pragma once

#include <api/types/MessageEntity.hpp>
#include <api/types/Message.hpp>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace api::types {

/**
 * @brief Describes reply parameters for the message that is being sent.
 */
struct ReplyParameters {
    /**
     * @brief Identifier of the message that will be replied to in the current
     * chat, or in the chat chatId if it is specified
     */
    std::int32_t messageId;

    /**
     * @brief Optional. If the message to be replied to is from a different
     * chat, unique identifier for the chat or username of the channel (in the
     * format @channelusername).
     *
     * Not supported for messages sent on behalf of a business account.
     */
    std::optional<std::int64_t> chatId;

    /**
     * @brief Optional. Pass True if the message should be sent even if the
     * specified message to be replied to is not found.
     *
     * Always False for replies in another chat or forum topic.
     * Always True for messages sent on behalf of a business account.
     */
    std::optional<bool> allowSendingWithoutReply;

    /**
     * @brief Optional. Quoted part of the message to be replied to; 0-1024
     * characters after entities parsing.
     *
     * The quote must be an exact substring of the message to be replied to,
     * including bold, italic, underline, strikethrough, spoiler, and
     * customEmoji entities. The message will fail to send if the quote isn't
     * found in the original message.
     */
    std::optional<std::string> quote;

    /**
     * @brief Optional. Mode for parsing entities in the quote.
     *
     * See [formatting
     * options](https://core.telegram.org/bots/api#formatting-options) for more
     * details.
     */
    std::optional<std::string> quoteParseMode;

    /**
     * @brief Optional. A JSON-serialized list of special entities that appear
     * in the quote.
     *
     * It can be specified instead of quoteParseMode.
     */
    std::optional<std::vector<MessageEntity>> quoteEntities;

    /**
     * @brief Optional. Position of the quote in the original message in UTF-16
     * code units
     */
    std::optional<std::int32_t> quotePosition;

    /**
     * @brief Optional. Thread identifier in which to send the message.
     *
     * For forum supergroups, the message will be sent to the specified topic
     * within the supergroup. For other chats, the message will be sent as a
     * regular reply.
     * 
     * API_EXTENSION: Custom extension for supporting message threads.
     */

    std::optional<Message::messageThreadId_type> messageThreadId;

    [[nodiscard]] bool hasThreadId() const {
        return messageThreadId.has_value();
    }
};

}  // namespace api::types
