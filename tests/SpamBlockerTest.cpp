#include "SingleThreadCtrlAccessors.h"
#include "tgbot/types/Chat.h"

#include <Authorization.h>
#include <SpamBlock.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <memory>
#include <vector>

using std::chrono_literals::operator""ms;
using std::chrono_literals::operator""s;
using testing::_;
using testing::Mock;

using Accessor = SingleThreadCtrlTestAccessors<SingleThreadCtrlManager::USAGE_SPAMBLOCK_THREAD>;

static User::Ptr createBlex() {
    auto blex = std::make_shared<User>();
    blex->username = "FlorinelulX123";
    blex->firstName = "Alex";
    blex->lastName = "Bomb";
    blex->isPremium = true;
    blex->isBot = true;
    return blex;
}

static Chat::Ptr createXRomGroup() {
    auto xrom = std::make_shared<Chat>();
    xrom->title = "YROM Official Group";
    xrom->username = "YromY";
    xrom->type = TgBot::Chat::Type::Supergroup;
    return xrom;
}

static auto genMsgBuf(size_t size) {
    std::vector<Message::Ptr> msgBuf(size);
    const auto blex = createBlex();
    const auto xrom = createXRomGroup();
    std::for_each(msgBuf.begin(), msgBuf.end(), [blex, xrom](Message::Ptr& it) {
        it = std::make_shared<Message>();
        it->from = blex;
        it->chat = xrom;
        it->text = "Bomb";
    });
    return msgBuf;
}

static void updateTimeStamp(std::vector<Message::Ptr>& inBuf, const std::chrono::system_clock::time_point tp) {
    std::for_each(inBuf.begin(), inBuf.end(), [tp](Message::Ptr& it) {
        it->date = std::chrono::system_clock::to_time_t(tp);
    });
}

struct MockSpamBlockTest : public SpamBlockBase {
    using SpamBlockBase::SpamBlockBase;
    MOCK_METHOD(void, handleUserAndMessagePair,
                (PerChatHandleConstRef, OneChatIterator, const size_t, const char*));
    void checks(PerChatHandleConstRef e, OneChatIterator it,
                const size_t th, const char* n) {
        ASSERT_TRUE(isEntryOverThreshold(e, th));
        _logSpamDetectCommon(e, n);
    }
};

TEST(SpamBlockTest, GeneralSpamEntryOverThreshold) {
    auto msgs = genMsgBuf(30);
    updateTimeStamp(msgs, std::chrono::system_clock::now());
    auto mgr = Accessor::createAndGet<MockSpamBlockTest>();
    // TODO: Check why the function is called twice; can it be different on other platforms or CPUs?
    EXPECT_CALL(*mgr, handleUserAndMessagePair(_, _, _, _))
        .Times(2)
        .WillRepeatedly([mgr](MockSpamBlockTest::PerChatHandleConstRef e,
                      MockSpamBlockTest::OneChatIterator it,
                      const size_t th, const char* n) {
            mgr->checks(e, it, th, n);
        });
    for (const auto& i : msgs)
        mgr->addMessage(i);
    std::this_thread::sleep_for(100ms);
    mgr->stop();
    Mock::VerifyAndClearExpectations(mgr.get());
    Accessor::destroy();
}

TEST(SpamBlockTest, GeneralSpamEntryOverThresholdOld) {
    auto msgs = genMsgBuf(10);
    updateTimeStamp(msgs, std::chrono::system_clock::now());
    auto mgr = Accessor::createAndGet<MockSpamBlockTest>();
    EXPECT_CALL(*mgr, handleUserAndMessagePair(_, _, _, _)).Times(0);
    for (const auto& i : msgs)
        mgr->addMessage(i);
    std::this_thread::sleep_for(100ms);
    mgr->stop();
    Mock::VerifyAndClearExpectations(mgr.get());
    Accessor::destroy();
}