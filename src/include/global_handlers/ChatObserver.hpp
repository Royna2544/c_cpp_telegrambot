#pragma once

#include <Types.h>
#include <tgbot/types/Message.h>

#include <vector>
#include "api/TgBotApi.hpp"
#include "trivial_helpers/fruit_inject.hpp"

using TgBot::Message;
using TgBot::User;

class ChatObserver {
    // Global ChatId list to observe
    std::vector<ChatId> observedChatIds;
    bool observeAllChats{};
    mutable std::mutex m;

   public:
    static void printChatMsg(const Message::Ptr& msg, const User::Ptr& from);
    /**
     * process - Process a msg and log the chat content
     *
     * The chat msgs will be logged if the ChatId is being 'observed'
     * @param msg message object to observe
     */
    void process(const Message::Ptr& msg);

    bool startObserving(ChatId chatId);
    bool stopObserving(ChatId chatId);
    bool observeAll(bool observe);

    APPLE_EXPLICIT_INJECT(ChatObserver(TgBotApi::Ptr api));

    explicit operator bool() const;
};
