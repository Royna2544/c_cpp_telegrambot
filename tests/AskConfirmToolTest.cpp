#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "AskConfirmTool.hpp"
#include "ModelPickerTool.hpp"
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

    EXPECT_CALL(api, submitCommandWork("ask", TgBotApi::WorkClass::Outbound,
                                       testing::_, testing::_))
        .WillOnce(testing::Invoke([](std::string_view, TgBotApi::WorkClass,
                                     TgBotApi::CancellableWork work,
                                     TgBotApi::WorkOptions) {
            work(std::stop_token{});
            return std::optional<TgBotApi::WorkId>{1};
        }));
    EXPECT_CALL(
        api,
        answerCallbackQuery_impl(
            "callback-id",
            "Only the initiating admin can answer this confirmation.", true))
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

TEST(AskConfirmToolTest, RecordsOnlyTheFirstCallbackForAToken) {
    MockTgBotApi api;
    constexpr UserId initiatingUser = 101;
    const std::string token = "single-use-test";
    auto pending = std::make_shared<llm::ask_confirm::PendingConfirmation>();
    pending->initiatingUserId = initiatingUser;
    {
        auto [mutex, confirmations] = llm::ask_confirm::confirmationRegistry();
        const std::lock_guard lock(mutex);
        confirmations.emplace(token, pending);
    }

    auto query = std::make_shared<TgBot::CallbackQuery>();
    query->id = "first-callback";
    query->data =
        std::string(llm::ask_confirm::kAskConfirmPrefix) + token + ":y";
    query->from = std::make_shared<TgBot::User>();
    query->from->id = initiatingUser;
    query->from->firstName = "Owner";

    EXPECT_CALL(api, submitCommandWork("ask", TgBotApi::WorkClass::Outbound,
                                       testing::_, testing::_))
        .Times(2)
        .WillRepeatedly(testing::Invoke(
            [](std::string_view, TgBotApi::WorkClass,
               TgBotApi::CancellableWork work, TgBotApi::WorkOptions) {
                work(std::stop_token{});
                return std::optional<TgBotApi::WorkId>{1};
            }));
    EXPECT_CALL(api,
                answerCallbackQuery_impl("first-callback", "Recorded.", false))
        .WillOnce(Return(true));
    llm::ask_confirm::handleCallback(&api, query);

    query->id = "second-callback";
    query->data =
        std::string(llm::ask_confirm::kAskConfirmPrefix) + token + ":n";
    EXPECT_CALL(api, answerCallbackQuery_impl(
                         "second-callback",
                         "This confirmation was already answered or has "
                         "expired.",
                         true))
        .WillOnce(Return(true));
    llm::ask_confirm::handleCallback(&api, query);

    {
        const std::lock_guard lock(pending->mtx);
        ASSERT_TRUE(pending->result.has_value());
        EXPECT_EQ(pending->result->choice, "y");
        EXPECT_TRUE(pending->closed);
    }
    auto [mutex, confirmations] = llm::ask_confirm::confirmationRegistry();
    const std::lock_guard lock(mutex);
    confirmations.erase(token);
}

TEST(ModelPickerToolTest, ReplacesGenerationAndBindsInitiatingUser) {
    MockTgBotApi api;
    constexpr ChatId chatId = 303;
    constexpr UserId firstUser = 101;
    constexpr UserId secondUser = 202;

    const auto firstKeyboard = llm::model_picker::startPicker(
        &api, chatId, firstUser, {"model-a", "model-b"});
    ASSERT_FALSE(firstKeyboard->inlineKeyboard.empty());
    ASSERT_FALSE(firstKeyboard->inlineKeyboard.front().empty());
    const auto firstCallback =
        firstKeyboard->inlineKeyboard.front().front()->callbackData;
    ASSERT_TRUE(firstCallback.has_value());

    std::string firstGeneration;
    {
        auto [mutex, pickers] = llm::model_picker::modelPickerStore();
        const std::lock_guard lock(mutex);
        const auto it = pickers.find(chatId);
        ASSERT_NE(it, pickers.end());
        EXPECT_EQ(it->second.initiatingUserId, firstUser);
        EXPECT_GT(it->second.expiresAt, std::chrono::steady_clock::now());
        firstGeneration = it->second.generation;
    }
    EXPECT_TRUE(firstCallback->starts_with(
        std::string(llm::model_picker::kCallbackPrefix) + firstGeneration +
        ":s:"));

    const auto secondKeyboard =
        llm::model_picker::startPicker(&api, chatId, secondUser, {"model-c"});
    ASSERT_FALSE(secondKeyboard->inlineKeyboard.empty());
    const auto secondCallback =
        secondKeyboard->inlineKeyboard.front().front()->callbackData;
    ASSERT_TRUE(secondCallback.has_value());

    {
        auto [mutex, pickers] = llm::model_picker::modelPickerStore();
        const std::lock_guard lock(mutex);
        const auto it = pickers.find(chatId);
        ASSERT_NE(it, pickers.end());
        EXPECT_EQ(it->second.initiatingUserId, secondUser);
        EXPECT_NE(it->second.generation, firstGeneration);
        EXPECT_TRUE(secondCallback->starts_with(
            std::string(llm::model_picker::kCallbackPrefix) +
            it->second.generation + ":s:"));
        pickers.erase(it);
    }
}

}  // namespace
