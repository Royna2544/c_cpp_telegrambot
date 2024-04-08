#pragma once

#include <tgbot/Bot.h>
#include <tgbot/types/Chat.h>
#include <tgbot/types/Message.h>
#include <tgbot/types/User.h>
#include <memory>

#include "CStringLifetime.h"
#include "SingleThreadCtrl.h"
#include "BotClassBase.h"
#include "initcalls/BotInitcall.hpp"
#include "OnAnyMessageRegister.hpp"

#ifdef SOCKET_CONNECTION
#include <socket/TgBotSocket.h>
#endif

#include <map>
#include <mutex>

#ifdef SOCKET_CONNECTION
using namespace TgBotCommandData;
extern CtrlSpamBlock gSpamBlockCfg;
#endif

using TgBot::Bot;
using TgBot::Chat;
using TgBot::Message;
using TgBot::User;

struct SpamBlockBase : SingleThreadCtrlRunnable<> {
    // User and array of message pointers sent by that user
    using PerChatHandle = std::map<User::Ptr, std::vector<Message::Ptr>>;
    // Iterator type of buffer object, which contains <chats <users <msgs>>> map
    using OneChatIterator = std::map<Chat::Ptr, PerChatHandle>::const_iterator;
    using PerChatHandleConstRef = PerChatHandle::const_reference;
    using SingleThreadCtrlRunnable::SingleThreadCtrlRunnable;
    constexpr static int sMaxSameMsgThreshold = 3;
    constexpr static int sMaxMsgThreshold = 5;
    constexpr static int sSpamDetectThreshold = 5;

    virtual ~SpamBlockBase() = default;
    virtual void handleUserAndMessagePair(PerChatHandleConstRef e,
                                          OneChatIterator it,
                                          const size_t threshold,
                                          const char *name){};

    void runFunction() override;
    void addMessage(const Message::Ptr &message);

    static std::string commonMsgdataFn(const Message::Ptr &m);

   protected:
    bool isEntryOverThreshold(PerChatHandleConstRef t, const size_t threshold);
    void _logSpamDetectCommon(PerChatHandleConstRef t, const char *name);

   private:
    void spamDetectFunc(OneChatIterator handle);
    void takeAction(OneChatIterator handle, const PerChatHandle &map,
                    const size_t threshold, const char *name);
    std::map<Chat::Ptr, PerChatHandle> buffer;
    std::map<Chat::Ptr, int> buffer_sub;
    std::mutex buffer_m;  // Protect buffer, buffer_sub
};

struct SpamBlockManager : SpamBlockBase, BotClassBase, BotInitCall {
    SpamBlockManager(const Bot &bot) : SpamBlockBase(), BotClassBase(bot) {}
    ~SpamBlockManager() override = default;

    using SpamBlockBase::run;
    using SpamBlockBase::runFunction;
    void handleUserAndMessagePair(PerChatHandleConstRef e, OneChatIterator it,
                                  const size_t threshold,
                                  const char *name) override;
    void doInitCall(Bot& bot) override {
        OnAnyMessageRegisterer::getInstance()
            .registerCallback([](const Bot &bot, const Message::Ptr &message) {
                static auto spamMgr =
                    SingleThreadCtrlManager::getInstance()
                        .getController<SpamBlockManager>(
                            {SingleThreadCtrlManager::USAGE_SPAMBLOCK_THREAD},
                            std::cref(bot));
                spamMgr->addMessage(message);
            });
    }
    const CStringLifetime getInitCallName() const override {
        return OnAnyMessageRegisterer::getInitCallNameForClient("SpamBlock");
    }

   private:
    constexpr static auto kMuteDuration = std::chrono::minutes(3);
    void _deleteAndMuteCommon(const OneChatIterator &handle,
                              PerChatHandleConstRef t, const size_t threshold,
                              const char *name, const bool mute);
};