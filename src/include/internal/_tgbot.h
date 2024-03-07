#pragma once

#include <tgbot/types/Chat.h>
#include <tgbot/types/User.h>

using TgBot::Chat;
using TgBot::User;

inline bool operator==(const Chat::Ptr thiz, const Chat::Ptr other) {
    return thiz->id == other->id;
}

inline std::string UserPtr_toString(const User::Ptr bro) {
    std::string username = bro->firstName;
    if (!bro->lastName.empty())
        username += ' ' + bro->lastName;
    return '\'' + username + '\'';
}

inline std::string ChatPtr_toString(const Chat::Ptr ch) {
    if (ch->type == TgBot::Chat::Type::Private)
        return "Bot PM";
    return ch->title;
}