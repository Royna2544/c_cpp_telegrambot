#pragma once

#include <Authorization.hpp>
#include <ManagedThreads.hpp>
#include <api/TgBotApi.hpp>
#include <map>
#include <mutex>
#include <socket/include/TgBotSocket_Export.hpp>
#include "trivial_helpers/fruit_inject.hpp"

using namespace TgBotSocket::data;

using TgBot::Chat;
using TgBot::Message;
using TgBot::User;

struct SpamBlockBase : ManagedThreadRunnable {
    // User and array of message pointers sent by that user
    using PerChatHandle = std::map<User::Ptr, std::vector<Message::Ptr>>;
    // Iterator type of buffer object, which contains <chats <users <msgs>>> map
    using OneChatIterator = std::map<Chat::Ptr, PerChatHandle>::const_iterator;
    using PerChatHandleConstRef = PerChatHandle::const_reference;
    using ManagedThreadRunnable::ManagedThreadRunnable;
    constexpr static int sMaxSameMsgThreshold = 3;
    constexpr static int sMaxMsgThreshold = 5;
    constexpr static int sSpamDetectThreshold = 5;

    ~SpamBlockBase() override = default;
    virtual void handleUserAndMessagePair(PerChatHandleConstRef e,
                                          OneChatIterator it,
                                          const size_t threshold,
                                          const char *name) {};
    virtual bool shouldBeSkipped(const Message::Ptr &msg) const = 0;

    void runFunction() override;
    void addMessage(const Message::Ptr &message);

    static std::string commonMsgdataFn(const Message::Ptr &m);

    CtrlSpamBlock spamBlockConfig = CtrlSpamBlock::CTRL_ON;

   protected:
    static bool isEntryOverThreshold(PerChatHandleConstRef t,
                                     const size_t threshold);
    static void _logSpamDetectCommon(PerChatHandleConstRef t, const char *name);

   private:
    void spamDetectFunc(OneChatIterator handle);
    void takeAction(OneChatIterator handle, const PerChatHandle &map,
                    const size_t threshold, const char *name);
    std::map<Chat::Ptr, PerChatHandle> buffer;
    std::map<Chat::Ptr, int> buffer_sub;
    std::mutex buffer_m;  // Protect buffer, buffer_sub
};

struct SpamBlockManager : SpamBlockBase {
    APPLE_INJECT(SpamBlockManager(TgBotApi::Ptr api, AuthContext *auth));
    ~SpamBlockManager() override = default;

    using SpamBlockBase::run;
    using SpamBlockBase::runFunction;
    void handleUserAndMessagePair(PerChatHandleConstRef e, OneChatIterator it,
                                  const size_t threshold,
                                  const char *name) override;
    // Additional hook for handling messages
    // that should be handled differently
    // (e.g., delete messages, mute users)
    bool shouldBeSkipped(const Message::Ptr &message) const override;

   private:
    constexpr static auto kMuteDuration = std::chrono::minutes(3);
    void _deleteAndMuteCommon(const OneChatIterator &handle,
                              PerChatHandleConstRef t, const size_t threshold,
                              const char *name, const bool mute);
    TgBotApi::Ptr _api;
    AuthContext *_auth;
};