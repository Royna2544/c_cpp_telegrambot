#include <gtest/gtest.h>

#include "ToolRouter.hpp"

namespace {

using llm::tool_router::Domain;
using llm::tool_router::PendingBuilds;

TEST(ToolRouterTest, RoutesExplicitKernelBuildWithoutClassifier) {
    EXPECT_EQ(llm::tool_router::deterministicRoute(
                  "Build the Eureka Kernel for a30 using defaults"),
              Domain::KernelBuild);
}

TEST(ToolRouterTest, RoutesExplicitRomBuildWithoutClassifier) {
    EXPECT_EQ(llm::tool_router::deterministicRoute(
                  "Prepare LineageOS Android 16 for a30"),
              Domain::RomBuild);
}

TEST(ToolRouterTest, LeavesDiscussionForClassifier) {
    EXPECT_FALSE(llm::tool_router::deterministicRoute(
        "How do I build an Android kernel?"));
    EXPECT_FALSE(
        llm::tool_router::deterministicRoute("What is the Eureka Kernel?"));
}

TEST(ToolRouterTest, LeavesUnrelatedBuildForClassifier) {
    EXPECT_FALSE(
        llm::tool_router::deterministicRoute("Build a small website for me"));
}

TEST(ToolRouterTest, PendingPlanRoutesShortFollowUp) {
    EXPECT_EQ(llm::tool_router::deterministicRoute(
                  "a30", PendingBuilds{.kernel = true}),
              Domain::KernelBuild);
    EXPECT_EQ(llm::tool_router::deterministicRoute("userdebug",
                                                   PendingBuilds{.rom = true}),
              Domain::RomBuild);
}

TEST(ToolRouterTest, ExplicitNewBuildOverridesPendingPlan) {
    EXPECT_EQ(llm::tool_router::deterministicRoute(
                  "Build LineageOS for a30", PendingBuilds{.kernel = true}),
              Domain::RomBuild);
}

TEST(ToolRouterTest, ParsesStrictAndDecoratedClassifierLabels) {
    EXPECT_EQ(llm::tool_router::parseClassifierResult("kernel_build"),
              Domain::KernelBuild);
    EXPECT_EQ(
        llm::tool_router::parseClassifierResult(R"({"label":"chat_registry"})"),
        Domain::ChatRegistry);
    EXPECT_EQ(llm::tool_router::parseClassifierResult(
                  "The correct label is telegram."),
              Domain::Telegram);
    EXPECT_FALSE(llm::tool_router::parseClassifierResult("unknown"));
    EXPECT_FALSE(llm::tool_router::parseClassifierResult("telegram or chat"));
}

}  // namespace
