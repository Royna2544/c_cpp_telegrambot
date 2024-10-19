#pragma once

#include <Types.h>
#include <tgbot/types/Message.h>

#include <vector>

#include <InstanceClassBase.hpp>

using TgBot::Message;
using TgBot::User;

struct ChatObserver : InstanceClassBase<ChatObserver> {
    // Global ChatId list to observe
    std::vector<ChatId> observedChatIds;
    bool observeAllChats;
    std::mutex m;

    static void printChatMsg(const Message::Ptr& msg, const User::Ptr& from);
    /**
     * process - Process a msg and log the chat content
     *
     * The chat msgs will be logged if the ChatId is being 'observed'
     * @param msg message object to observe
     */
    void process(const Message::Ptr& msg);
};
