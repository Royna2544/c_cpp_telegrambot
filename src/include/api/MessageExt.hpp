#pragma once

#include <tgbot/types/Message.h>

#include <algorithm>
#include <chrono>
#include <initializer_list>
#include <memory>
#include <optional>
#include <type_traits>

#include "api/typedefs.h"

using TgBot::Animation;
using TgBot::Chat;
using TgBot::Message;
using TgBot::PhotoSize;
using TgBot::Sticker;
using TgBot::User;
using TgBot::Video;

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
    using type = MessageId;
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
    using type = TgBot::Document::Ptr;
};

}  // namespace internal::message

class MessageExt {
   public:
    using Ptr = std::add_pointer_t<MessageExt>;
    using AttrList = std::initializer_list<MessageAttrs>;
    template <typename T>
    using MakeCRef = std::add_lvalue_reference_t<std::add_const_t<T>>;

    explicit MessageExt(Message::Ptr message,
                        SplitMessageText how = SplitMessageText::None);

    // Get an attribute value
    template <MessageAttrs attr>
    [[nodiscard]] typename internal::message::AttributeType<attr>::type get()
        const {
        if (!has<attr>()) {
            return {};
        }
        if constexpr (attr == MessageAttrs::ExtraText) {
            return _extra_args;
        } else if constexpr (attr == MessageAttrs::Photo) {
            return _message->photo.back();
        } else if constexpr (attr == MessageAttrs::Sticker) {
            return _message->sticker;
        } else if constexpr (attr == MessageAttrs::Animation) {
            return _message->animation;
        } else if constexpr (attr == MessageAttrs::User) {
            return _message->from;
        } else if constexpr (attr == MessageAttrs::Chat) {
            return _message->chat;
        } else if constexpr (attr == MessageAttrs::BotCommand) {
            return command.value();
        } else if constexpr (attr == MessageAttrs::ParsedArgumentsList) {
            return _arguments;
        } else if constexpr (attr == MessageAttrs::Date) {
            return std::chrono::system_clock::from_time_t(_message->date);
        } else if constexpr (attr == MessageAttrs::MessageId) {
            return _message->messageId;
        } else if constexpr (attr == MessageAttrs::Video) {
            return _message->video;
        } else if constexpr (attr == MessageAttrs::Locale) {
            std::string loc{};
            if (has<MessageAttrs::User>()) {
                loc = get<MessageAttrs::User>()->languageCode.value_or("");
            }
            return loc;
        } else if constexpr (attr == MessageAttrs::Document) {
            return _message->document;
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
        if (!_message) {
            return false;
        }
        return std::ranges::any_of(
            attrs, [this](const auto& attr) { return has_attribute(attr); });
    }

    // Equality comparsion.
    template <MessageAttrs attr>
    bool is(MakeCRef<typename internal::message::AttributeType<attr>::type>
                object) const {
        if (!has({attr})) {
            return false;
        }
        return get<attr>() == object;
    }

    [[nodiscard]] bool exists() const { return _message != nullptr; }

    [[nodiscard]] const Message::Ptr& message() const { return _message; }
    [[nodiscard]] MessageExt::Ptr reply() const { return _replyMessage.get(); }

   private:
    // Like /start@some_bot
    std::optional<internal::message::BotCommand> command;
    // Additional arguments following the bot command
    std::string _extra_args;
    // Splitted arguments
    std::vector<std::string> _arguments;
    // A reference to the original message
    Message::Ptr _message;
    // Reference to the message that this is a reply to (if any)
    std::shared_ptr<MessageExt> _replyMessage;

    [[nodiscard]] bool has_attribute(const MessageAttrs& attr) const {
        if (!_message) {
            return false;
        }
        switch (attr) {
            case MessageAttrs::ExtraText:
                return !_extra_args.empty();
            case MessageAttrs::Photo:
                return !_message->photo.empty();
            case MessageAttrs::Sticker:
                return _message->sticker != nullptr;
            case MessageAttrs::Animation:
                return _message->animation != nullptr;
            case MessageAttrs::User:
            case MessageAttrs::Locale:
                return _message->from != nullptr;
            case MessageAttrs::Chat:
                return _message->chat != nullptr;
            case MessageAttrs::BotCommand:
                return command.has_value();
            case MessageAttrs::ParsedArgumentsList:
                return !_arguments.empty();
            case MessageAttrs::Video:
                return _message->video != nullptr;
            case MessageAttrs::Date:
            case MessageAttrs::MessageId:
                return true;
            case MessageAttrs::Document:
                return _message->document != nullptr;
        }
        return false;
    }
};
