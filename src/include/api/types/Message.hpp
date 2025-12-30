#pragma once

#include <algorithm>
#include <chrono>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include "Chat.hpp"
#include "Media.hpp"
#include "User.hpp"
#include "forwards.hpp"

namespace tgbot_api {

// Split type to obtain arguments.
enum class SplitMessageText {
    None,
    ByWhitespace,
    ByComma,
    ByNewline,
};

enum class MessageAttrs {
    ExtraText,
    Photo,
    Sticker,
    Animation,
    User,
    Chat,
    BotCommand,
    ParsedArgumentsList,
    Date,
    MessageId,
    Video,
    Locale,
    Document
};

inline std::ostream& operator<<(std::ostream& stream,
                                const MessageAttrs& attrs) {
    switch (attrs) {
        case MessageAttrs::ExtraText:
            return stream << "ExtraText";
        case MessageAttrs::Photo:
            return stream << "Photo";
        case MessageAttrs::Sticker:
            return stream << "Sticker";
        case MessageAttrs::Animation:
            return stream << "Animation";
        case MessageAttrs::User:
            return stream << "User";
        case MessageAttrs::Chat:
            return stream << "Chat";
        case MessageAttrs::BotCommand:
            return stream << "BotCommand";
        case MessageAttrs::ParsedArgumentsList:
            return stream << "ParsedArgumentsList";
        case MessageAttrs::Date:
            return stream << "Date";
        case MessageAttrs::MessageId:
            return stream << "MessageId";
        case MessageAttrs::Video:
            return stream << "Video";
        case MessageAttrs::Locale:
            return stream << "Locale";
        case MessageAttrs::Document:
            return stream << "Document";
    }
    return stream;
}

namespace internal::message {

// A struct to contain the /start@some_bot parsed.
struct BotCommand {
    // e.g. start in /start@some_bot
    std::string name;
    // e.g. some_bot in /start@some_bot
    std::string target;
};

template <MessageAttrs T>
struct AttributeType;

template <>
struct AttributeType<MessageAttrs::Animation> {
    using type = Animation::Ptr;
};
template <>
struct AttributeType<MessageAttrs::Sticker> {
    using type = Sticker::Ptr;
};
template <>
struct AttributeType<MessageAttrs::Photo> {
    using type = PhotoSize::Ptr;
};
template <>
struct AttributeType<MessageAttrs::ExtraText> {
    using type = std::string;
};
template <>
struct AttributeType<MessageAttrs::User> {
    using type = User::Ptr;
};
template <>
struct AttributeType<MessageAttrs::Chat> {
    using type = Chat::Ptr;
};
template <>
struct AttributeType<MessageAttrs::BotCommand> {
    using type = BotCommand;
};
template <>
struct AttributeType<MessageAttrs::ParsedArgumentsList> {
    using type = std::vector<std::string>;
};
template <>
struct AttributeType<MessageAttrs::Date> {
    using type = std::chrono::system_clock::time_point;
};
template <>
struct AttributeType<MessageAttrs::MessageId> {
    using type = tgbot_api::MessageId;
};
template <>
struct AttributeType<MessageAttrs::Video> {
    using type = Video::Ptr;
};
template <>
struct AttributeType<MessageAttrs::Locale> {
    using type = std::string;
};
template <>
struct AttributeType<MessageAttrs::Document> {
    using type = Document::Ptr;
};

}  // namespace internal::message

/**
 * @brief This object represents a message with extended parsing capabilities.
 *
 * This class combines the standard Telegram Message with additional parsing
 * features that were previously provided by MessageExt. It supports extracting
 * bot commands, arguments, and various media types from messages.
 */
class Message {
   public:
    using Ptr = std::shared_ptr<Message>;
    using AttrList = std::initializer_list<MessageAttrs>;
    template <typename T>
    using MakeCRef = std::add_lvalue_reference_t<std::add_const_t<T>>;

    // Standard Telegram message fields
    MessageId messageId{};
    std::optional<MessageThreadId> messageThreadId;
    User::Ptr from;
    Chat::Ptr senderChat;
    ChatId chat_id{};  // Denormalized for convenience
    std::int32_t date{};
    Chat::Ptr chat;
    std::optional<std::string> text;
    std::vector<MessageEntity::Ptr> entities;
    std::vector<PhotoSize::Ptr> photo;
    Animation::Ptr animation;
    Document::Ptr document;
    Sticker::Ptr sticker;
    Video::Ptr video;
    Message::Ptr replyToMessage;
    std::optional<bool> isTopicMessage;

    // Extended parsing fields
    std::optional<internal::message::BotCommand> command;
    std::string extraArgs;
    std::vector<std::string> arguments;
    std::shared_ptr<Message> parsedReplyMessage;

    /**
     * @brief Constructs a Message from basic fields
     */
    Message() = default;

    /**
     * @brief Parses the message text to extract commands and arguments
     *
     * @param how How to split the message text into arguments
     */
    void parseText(SplitMessageText how = SplitMessageText::None);

    // Get an attribute value
    template <MessageAttrs attr>
    [[nodiscard]] typename internal::message::AttributeType<attr>::type get()
        const {
        if (!has<attr>()) {
            return {};
        }
        if constexpr (attr == MessageAttrs::ExtraText) {
            return extraArgs;
        } else if constexpr (attr == MessageAttrs::Photo) {
            return photo.back();
        } else if constexpr (attr == MessageAttrs::Sticker) {
            return sticker;
        } else if constexpr (attr == MessageAttrs::Animation) {
            return animation;
        } else if constexpr (attr == MessageAttrs::User) {
            return from;
        } else if constexpr (attr == MessageAttrs::Chat) {
            return chat;
        } else if constexpr (attr == MessageAttrs::BotCommand) {
            return command.value();
        } else if constexpr (attr == MessageAttrs::ParsedArgumentsList) {
            return arguments;
        } else if constexpr (attr == MessageAttrs::Date) {
            return std::chrono::system_clock::from_time_t(date);
        } else if constexpr (attr == MessageAttrs::MessageId) {
            return messageId;
        } else if constexpr (attr == MessageAttrs::Video) {
            return video;
        } else if constexpr (attr == MessageAttrs::Locale) {
            std::string loc{};
            if (has<MessageAttrs::User>()) {
                loc = get<MessageAttrs::User>()->languageCode.value_or("");
            }
            return loc;
        } else if constexpr (attr == MessageAttrs::Document) {
            return document;
        }
        return {};
    }

    template <MessageAttrs attr>
    [[nodiscard]] typename internal::message::AttributeType<attr>::type get_or(
        typename internal::message::AttributeType<attr>::type default_value)
        const {
        if (!has<attr>()) {
            return default_value;
        }
        return get<attr>();
    }

    // Has an attribute?
    [[nodiscard]] bool has(const AttrList& attrs) const {
        return std::ranges::all_of(
            attrs, [this](const auto& attr) { return has_attribute(attr); });
    }

    template <MessageAttrs... attr>
    [[nodiscard]] bool has() const {
        return has({attr...});
    }

    // Has any of the following attributes?
    [[nodiscard]] bool any(const AttrList& attrs) const {
        return std::ranges::any_of(
            attrs, [this](const auto& attr) { return has_attribute(attr); });
    }

    // Equality comparison.
    template <MessageAttrs attr>
    bool is(MakeCRef<typename internal::message::AttributeType<attr>::type>
                object) const {
        if (!has({attr})) {
            return false;
        }
        return get<attr>() == object;
    }

    [[nodiscard]] bool exists() const { return messageId != 0; }

    [[nodiscard]] Message::Ptr reply() const { return parsedReplyMessage; }

   private:
    [[nodiscard]] bool has_attribute(const MessageAttrs& attr) const {
        switch (attr) {
            case MessageAttrs::ExtraText:
                return !extraArgs.empty();
            case MessageAttrs::Photo:
                return !photo.empty();
            case MessageAttrs::Sticker:
                return sticker != nullptr;
            case MessageAttrs::Animation:
                return animation != nullptr;
            case MessageAttrs::User:
            case MessageAttrs::Locale:
                return from != nullptr;
            case MessageAttrs::Chat:
                return chat != nullptr;
            case MessageAttrs::BotCommand:
                return command.has_value();
            case MessageAttrs::ParsedArgumentsList:
                return !arguments.empty();
            case MessageAttrs::Video:
                return video != nullptr;
            case MessageAttrs::Date:
            case MessageAttrs::MessageId:
                return true;
            case MessageAttrs::Document:
                return document != nullptr;
        }
        return false;
    }
};

}  // namespace tgbot_api
