#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "AskConfirmTool.hpp"
#include "mocks/TgBotApi.hpp"

using testing::Return;

namespace {

TEST(AskConfirmToolTest, RejectsCallbackFromDifferentUser) {
    MockTgBotApi api;
    constexpr UserId initiatingUser = 101;
    constexpr UserId otherUser = 202;
    const std::string token = "confirmation-test";
    auto pending = std::make_shared<llm::ask_confirm::PendingConfirmation>();
    pending->initiatingUserId = initiatingUser;

    {
        auto [mutex, confirmations] = llm::ask_confirm::confirmationRegistry();
        const std::lock_guard lock(mutex);
        confirmations.emplace(token, pending);
    }

    auto query = std::make_shared<TgBot::CallbackQuery>();
    query->id = "callback-id";
    query->data =
        std::string(llm::ask_confirm::kAskConfirmPrefix) + token + ":y";
    query->from = std::make_shared<TgBot::User>();
    query->from->id = otherUser;
    query->from->firstName = "Intruder";

    EXPECT_CALL(api,
                answerCallbackQuery_impl("callback-id", "Only the initiating admin can answer this confirmation.", true))
        .WillOnce(Return(true));

    llm::ask_confirm::handleCallback(&api, query);

    {
        const std::lock_guard lock(pending->mtx);
        EXPECT_FALSE(pending->result.has_value());
    }

    auto [mutex, confirmations] = llm::ask_confirm::confirmationRegistry();
    const std::lock_guard lock(mutex);
    confirmations.erase(token);
}

}  // namespace
