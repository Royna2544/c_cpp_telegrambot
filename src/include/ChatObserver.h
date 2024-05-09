#pragma once

#include <Types.h>
#include <tgbot/types/Message.h>

#include <vector>

#include "CStringLifetime.h"
#include "InstanceClassBase.hpp"
#include "OnAnyMessageRegister.hpp"
#include "initcalls/Initcall.hpp"

using TgBot::Message;
using TgBot::User;

struct ChatObserver : InitCall, InstanceClassBase<ChatObserver> {
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

    void doInitCall() override;
    const CStringLifetime getInitCallName() const override {
        return OnAnyMessageRegisterer::getInitCallNameForClient("ChatObserver");
    }
};
