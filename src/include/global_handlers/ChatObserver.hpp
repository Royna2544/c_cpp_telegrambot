#pragma once

#include <api/types/Chat.hpp>
#include <api/types/Message.hpp>
#include <api/types/User.hpp>
#include <api/TgBotApi.hpp>

#include <mutex>
#include <unordered_set>
#include "trivial_helpers/fruit_inject.hpp"

class ChatObserver {
    // Global ChatId list to observe
    std::unordered_set<api::types::Chat::id_type> observedChatIds;
    bool observeAllChats{};
    mutable std::mutex m;

   public:
    static void printChatMsg(const api::types::Message& msg,
                             const api::types::User& from);

    /**
     * process - Process a msg and log the chat content
     *
     * The chat msgs will be logged if the ChatId is being 'observed'
     * @param msg message object to observe
     */
    void process(const api::types::Message& msg);

    bool startObserving(api::types::Chat::id_type chatId);
    bool stopObserving(api::types::Chat::id_type chatId);
    bool observeAll(bool observe);

    APPLE_EXPLICIT_INJECT(ChatObserver(TgBotApi::Ptr api));

    explicit operator bool() const;
};
