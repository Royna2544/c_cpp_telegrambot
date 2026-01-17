#pragma once

#include <strutil.h>

#include <api/types/Animation.hpp>
#include <api/types/Chat.hpp>
#include <api/types/Document.hpp>
#include <api/types/Message.hpp>
#include <api/types/PhotoSize.hpp>
#include <api/types/Sticker.hpp>
#include <api/types/User.hpp>
#include <api/types/Video.hpp>
#include <chrono>
#include <initializer_list>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

namespace api::types {

namespace internal {

// Split type to obtain arguments.
enum class SplitMethod {
    None,
    ByWhitespace,
    ByComma,
    ByNewline,
};

// Attributes that can be extracted from the message.
enum class Attrs {
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

// A struct to contain the /start@some_bot parsed.
struct BotCommand {
    // e.g. start in /start@some_bot
    std::string name;
    // e.g. some_bot in /start@some_bot
    std::string target;
};

template <internal::Attrs T>
struct AttributeType;

template <>
struct AttributeType<internal::Attrs::Animation> {
    using type = Animation;
};
template <>
struct AttributeType<internal::Attrs::Sticker> {
    using type = Sticker;
};
template <>
struct AttributeType<internal::Attrs::Photo> {
    using type = PhotoSize;
};
template <>
struct AttributeType<internal::Attrs::ExtraText> {
    using type = std::string;
};
template <>
struct AttributeType<internal::Attrs::User> {
    using type = User;
};
template <>
struct AttributeType<internal::Attrs::Chat> {
    using type = Chat;
};
template <>
struct AttributeType<internal::Attrs::BotCommand> {
    using type = BotCommand;
};
template <>
struct AttributeType<internal::Attrs::ParsedArgumentsList> {
    using type = std::vector<std::string>;
};
template <>
struct AttributeType<internal::Attrs::Date> {
    using type = std::chrono::system_clock::time_point;
};
template <>
struct AttributeType<internal::Attrs::MessageId> {
    using type = Message::messageId_type;
};
template <>
struct AttributeType<internal::Attrs::Video> {
    using type = Video;
};
template <>
struct AttributeType<internal::Attrs::Locale> {
    using type = std::string;
};
template <>
struct AttributeType<internal::Attrs::Document> {
    using type = Document;
};

}  // namespace internal

/**
 * @brief This object represents a parsed message.
 *
 * API_ADDED: Extension for Message parsing
 */
struct ParsedMessage : public Message {
    using SplitMethod = internal::SplitMethod;
    using Attrs = internal::Attrs;
    using BotCommand = internal::BotCommand;

    using AttrList = std::initializer_list<internal::Attrs>;
    template <typename T>
    using MakeCRef = std::add_lvalue_reference_t<std::add_const_t<T>>;

    ParsedMessage(Message message, SplitMethod how) : Message(message) {
        // Empty message won't need parsing
        if (!text || !entities) {
            return;
        }

        // Initially, _extra_args is full text
        _extra_args = text.value();

        // Try to find botcommand entity
        const auto botCommandEnt =
            std::ranges::find_if(*entities, [](const auto& entity) {
                return entity.type ==
                           api::types::MessageEntity::Type::BotCommand &&
                       entity.offset == 0;
            });

        // I believe entity must be sent here.
        if (botCommandEnt != (*entities).end() && text->front() == '/') {
            const auto entry = *botCommandEnt;
            // Grab /start@username
            _extra_args = text->substr(entry.length);
            strutil::trim(_extra_args);
            command.emplace();

            auto elements = strutil::split(text->substr(1, entry.length), "@");
            switch (elements.size()) {
                case 0:
                    // Impossible
                    break;
                case 1:
                    command->name = elements[0];
                    command->target = "";
                    break;
                case 2:
                    command->name = elements[0];
                    command->target = elements[1];
                    break;
                default:
                    // Malformed.
                    break;
            }
        }

        if (_extra_args.size() != 0) {
            switch (how) {
                case SplitMethod::ByWhitespace:
                    _arguments = strutil::split(_extra_args, ' ');
                    break;
                case SplitMethod::ByComma:
                    _arguments = strutil::split(_extra_args, ',');
                    break;
                case SplitMethod::None:
                    // No-op, considering one argument.
                    _arguments.emplace_back(_extra_args);
                    break;
                case SplitMethod::ByNewline:
                    _arguments = strutil::split(_extra_args, '\n');
                    break;
            }
            for (auto& x : _arguments) {
                strutil::trim(x);
            }
        }
    }

    // Get an attribute value
    template <internal::Attrs attr>
    [[nodiscard]] typename internal::AttributeType<attr>::type get() const {
        if (!has<attr>()) {
            return {};
        }
        if constexpr (attr == internal::Attrs::ExtraText) {
            return _extra_args;
        } else if constexpr (attr == internal::Attrs::Photo) {
            return photo->back();
        } else if constexpr (attr == internal::Attrs::Sticker) {
            return sticker;
        } else if constexpr (attr == internal::Attrs::Animation) {
            return animation;
        } else if constexpr (attr == internal::Attrs::User) {
            return from;
        } else if constexpr (attr == internal::Attrs::Chat) {
            return chat;
        } else if constexpr (attr == internal::Attrs::BotCommand) {
            return command.value();
        } else if constexpr (attr == internal::Attrs::ParsedArgumentsList) {
            return _arguments;
        } else if constexpr (attr == internal::Attrs::Date) {
            return std::chrono::system_clock::from_time_t(date);
        } else if constexpr (attr == internal::Attrs::MessageId) {
            return messageId;
        } else if constexpr (attr == internal::Attrs::Video) {
            return video;
        } else if constexpr (attr == internal::Attrs::Locale) {
            std::string loc{};
            if (has<internal::Attrs::User>()) {
                loc = get<internal::Attrs::User>().languageCode.value_or("");
            }
            return loc;
        } else if constexpr (attr == internal::Attrs::Document) {
            return document;
        }
        return {};
    }

    template <internal::Attrs attr>
    [[nodiscard]] typename internal::AttributeType<attr>::type get_or(
        typename internal::AttributeType<attr>::type default_value) const {
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

    template <internal::Attrs... attr>
    [[nodiscard]] bool has() const {
        return has({attr...});
    }

    // Has any of the following attributes?
    [[nodiscard]] bool any(const AttrList& attrs) const {
        return std::ranges::any_of(
            attrs, [this](const auto& attr) { return has_attribute(attr); });
    }

    // Equality comparsion.
    template <internal::Attrs attr>
    bool is(
        MakeCRef<typename internal::AttributeType<attr>::type> object) const {
        if (!has({attr})) {
            return false;
        }
        return get<attr>() == object;
    }

   private:
    // Like /start@some_bot
    std::optional<internal::BotCommand> command;
    // Additional arguments following the bot command
    std::string _extra_args;
    // Splitted arguments
    std::vector<std::string> _arguments;

    [[nodiscard]] bool has_attribute(const internal::Attrs& attr) const {
        switch (attr) {
            case internal::Attrs::ExtraText:
                return !_extra_args.empty();
            case internal::Attrs::Photo:
                return photo.has_value();
            case internal::Attrs::Sticker:
                return sticker.has_value();
            case internal::Attrs::Animation:
                return animation.has_value();
            case internal::Attrs::User:
            case internal::Attrs::Locale:
                return from.has_value();
            case internal::Attrs::BotCommand:
                return command.has_value();
            case internal::Attrs::ParsedArgumentsList:
                return !_arguments.empty();
            case internal::Attrs::Video:
                return video.has_value();
            case internal::Attrs::Date:
            case internal::Attrs::MessageId:
            case internal::Attrs::Chat:
                return true;
            case internal::Attrs::Document:
                return document.has_value();
        }
        return false;
    }
};

}  // namespace api::types

inline std::ostream& operator<<(std::ostream& stream,
                                const api::types::internal::Attrs& attrs) {
    switch (attrs) {
        case api::types::internal::Attrs::ExtraText:
            return stream << "ExtraText";
        case api::types::internal::Attrs::Photo:
            return stream << "Photo";
        case api::types::internal::Attrs::Sticker:
            return stream << "Sticker";
        case api::types::internal::Attrs::Animation:
            return stream << "Animation";
        case api::types::internal::Attrs::User:
            return stream << "User";
        case api::types::internal::Attrs::Chat:
            return stream << "Chat";
        case api::types::internal::Attrs::BotCommand:
            return stream << "BotCommand";
        case api::types::internal::Attrs::ParsedArgumentsList:
            return stream << "ParsedArgumentsList";
        case api::types::internal::Attrs::Date:
            return stream << "Date";
        case api::types::internal::Attrs::MessageId:
            return stream << "MessageId";
        case api::types::internal::Attrs::Video:
            return stream << "Video";
        case api::types::internal::Attrs::Locale:
            return stream << "Locale";
        case api::types::internal::Attrs::Document:
            return stream << "Document";
    }
    return stream;
}
