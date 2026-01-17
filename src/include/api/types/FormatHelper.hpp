#pragma once

#include <fmt/format.h>
#include <api/types/Chat.hpp>
#include <api/types/User.hpp>

template <>
struct fmt::formatter<api::types::User> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx);

    template <typename FormatContext>
    auto format(api::types::User const& user, FormatContext& ctx) const;
};

template <typename ParseContext>
constexpr auto fmt::formatter<api::types::User>::parse(ParseContext& ctx) {
    return ctx.begin();
}

template <typename FormatContext>
auto fmt::formatter<api::types::User>::format(const api::types::User& user,
                                       FormatContext& ctx) const {
    if (user.lastName) {
        return fmt::format_to(ctx.out(), "{} {} (id: {})", user.firstName,
                              user.lastName.value(), user.id);
    }
    return fmt::format_to(ctx.out(), "{} (id: {})", user.firstName, user.id);
}

template <>
struct fmt::formatter<api::types::Chat> : formatter<std::string> {
    // parse is inherited from formatter<string_view>.
    static auto format(const api::types::Chat& chat, format_context& ctx)
        -> format_context::iterator {
        std::string name;
        if (chat.title) {
            name = *chat.title;
        } else if (chat.username) {
            name = fmt::format("@{}", *chat.username);
        } else {
            name = fmt::format("id: {}", chat.id);
        }
        switch (chat.type) {
            case api::types::Chat::Type::Private:
                return fmt::format_to(ctx.out(), "Private chat ({})", name);
            case api::types::Chat::Type::Channel:
                return fmt::format_to(ctx.out(), "Channel ({})", name);
            case api::types::Chat::Type::Supergroup:
                return fmt::format_to(ctx.out(), "Group ({})", name);
            case api::types::Chat::Type::Group:
                return fmt::format_to(ctx.out(), "Private Group ({})", name);
            default:
                return fmt::format_to(ctx.out(), "Unknown chat");
        }
    }
};
