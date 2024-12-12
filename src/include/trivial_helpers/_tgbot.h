#pragma once

#include <fmt/format.h>
#include <tgbot/types/Chat.h>
#include <tgbot/types/User.h>

using TgBot::Chat;
using TgBot::User;

template <>
struct fmt::formatter<User::Ptr> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx);

    template <typename FormatContext>
    auto format(User::Ptr const& user, FormatContext& ctx) const;
};

template <typename ParseContext>
constexpr auto fmt::formatter<User::Ptr>::parse(ParseContext& ctx) {
    return ctx.begin();
}

template <typename FormatContext>
auto fmt::formatter<User::Ptr>::format(const User::Ptr& user,
                                       FormatContext& ctx) const {
    if (user->lastName) {
        return fmt::format_to(ctx.out(), "{} {} (id: {})", user->firstName,
                              user->lastName.value(), user->id);
    }
    return fmt::format_to(ctx.out(), "{} (id: {})", user->firstName, user->id);
}

template <>
struct fmt::formatter<Chat::Ptr> : formatter<std::string> {
    // parse is inherited from formatter<string_view>.
    static auto format(const Chat::Ptr& chat,
                       format_context& ctx) -> format_context::iterator {
        std::string name;
        if (chat->title) {
            name = *chat->title;
        } else if (chat->username) {
            name = fmt::format("@{}", *chat->username);
        } else {
            name = fmt::format("id: {}", chat->id);
        }
        switch (chat->type) {
            case Chat::Type::Private:
                return fmt::format_to(ctx.out(), "Private chat ({})", name);
            case Chat::Type::Channel:
                return fmt::format_to(ctx.out(), "Channel ({})", name);
            case Chat::Type::Supergroup:
                return fmt::format_to(ctx.out(), "Group ({})", name);
            case Chat::Type::Group:
                return fmt::format_to(ctx.out(), "Private Group ({})", name);
            default:
                return fmt::format_to(ctx.out(), "Unknown chat");
        }
    }
};
