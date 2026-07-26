#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>

#include "BuilderLaunchTool.hpp"
#include "mocks/TgBotApi.hpp"

using testing::_;
using testing::Return;
using testing::SaveArg;

namespace {

TgBot::Message::Ptr makeMessage(std::int64_t id) {
    auto message = std::make_shared<TgBot::Message>();
    message->chat = std::make_shared<TgBot::Chat>();
    message->chat->id = id;
    message->from = std::make_shared<TgBot::User>();
    (*message->from)->id = id + 1000;
    return message;
}

TEST(BuilderLaunchToolTest, RetainsPartialKernelPlanAndReportsMissingField) {
    MockTgBotApi api;
    auto message = makeMessage(42);
    auto execute = llm::builder_launch::makeExecutor(&api, message);

    bool isError = true;
    const auto result =
        execute("kernelbuild", {{"kernel", "Eureka Kernel"}}, isError);

    EXPECT_FALSE(isError);
    EXPECT_THAT(result, testing::HasSubstr("device"));
    EXPECT_THAT(llm::builder_launch::pendingContext(message->chat->id,
                                                    (*message->from)->id),
                testing::HasSubstr("Eureka Kernel"));
}

TEST(BuilderLaunchToolTest, MergesFollowUpAndStagesKernelPlan) {
    MockTgBotApi api;
    auto message = makeMessage(43);
    auto execute = llm::builder_launch::makeExecutor(&api, message);
    std::string payload;
    EXPECT_CALL(api, invokeCommand("kernelbuild", message, _))
        .WillOnce(testing::DoAll(SaveArg<2>(&payload), Return(true)));

    bool isError = false;
    const auto partial =
        execute("kernelbuild", {{"kernel", "Eureka Kernel"}}, isError);
    ASSERT_FALSE(isError);
    EXPECT_THAT(partial, testing::HasSubstr("incomplete"));

    const auto result = execute("kernelbuild", {{"device", "a30"}}, isError);

    EXPECT_FALSE(isError);
    EXPECT_THAT(result, testing::HasSubstr("review and confirm"));
    const auto parsed = nlohmann::json::parse(payload);
    EXPECT_EQ(parsed.at("kernel"), "Eureka Kernel");
    EXPECT_EQ(parsed.at("device"), "a30");
    EXPECT_EQ(parsed.at("source"), "llm");
    EXPECT_TRUE(llm::builder_launch::pendingContext(message->chat->id,
                                                    (*message->from)->id)
                    .empty());
}

TEST(BuilderLaunchToolTest, StagesCompleteRomPlanWithOptions) {
    MockTgBotApi api;
    auto message = makeMessage(44);
    auto execute = llm::builder_launch::makeExecutor(&api, message);
    std::string payload;
    EXPECT_CALL(api, invokeCommand("rombuild", message, _))
        .WillOnce(testing::DoAll(SaveArg<2>(&payload), Return(true)));

    bool isError = true;
    const auto result = execute("rombuild",
                                {{"device", "a30"},
                                 {"rom", "LineageOS"},
                                 {"android_version", 16},
                                 {"variant", "userdebug"},
                                 {"repo_sync", false},
                                 {"upload", "stream"},
                                 {"parallel_jobs", 12}},
                                isError);

    EXPECT_FALSE(isError);
    EXPECT_THAT(result, testing::HasSubstr("review and confirm"));
    const auto parsed = nlohmann::json::parse(payload);
    EXPECT_EQ(parsed.at("rom"), "LineageOS");
    EXPECT_EQ(parsed.at("variant"), "userdebug");
    EXPECT_EQ(parsed.at("parallel_jobs"), 12);
}

TEST(BuilderLaunchToolTest, ReportsRejectedCompleteDispatch) {
    MockTgBotApi api;
    auto message = makeMessage(45);
    EXPECT_CALL(api, invokeCommand("kernelbuild", message, _))
        .WillOnce(Return(false));

    auto execute = llm::builder_launch::makeExecutor(&api, message);
    bool isError = false;
    const auto result = execute(
        "kernelbuild", {{"kernel", "Grand Kernel"}, {"device", "baffinvektt"}},
        isError);

    EXPECT_TRUE(isError);
    EXPECT_THAT(result, testing::HasSubstr("Failed to stage"));
}

TEST(BuilderLaunchToolTest, RejectsInvalidOptionType) {
    MockTgBotApi api;
    auto execute = llm::builder_launch::makeExecutor(&api, makeMessage(46));
    bool isError = false;
    const auto result = execute("rombuild", {{"parallel_jobs", 0}}, isError);

    EXPECT_TRUE(isError);
    EXPECT_THAT(result, testing::HasSubstr("positive integer"));
}

}  // namespace
