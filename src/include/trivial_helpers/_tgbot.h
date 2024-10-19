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
    if (!user->lastName.empty()) {
        return fmt::format_to(ctx.out(), "{} {} (id: {})", user->firstName,
                              user->lastName, user->id);
    }
    return fmt::format_to(ctx.out(), "{} (id: {})", user->firstName, user->id);
}

template <>
struct fmt::formatter<Chat::Ptr> : formatter<string_view> {
    // parse is inherited from formatter<string_view>.
    auto format(const Chat::Ptr& chat,
                format_context& ctx) const -> format_context::iterator {
        string_view name;
        switch (chat->type) {
            case Chat::Type::Private:
                name = "Private chat";
                break;
            default:
                name = chat->title;
                break;
        }
        return formatter<string_view>::format(name, ctx);
    }
};
